#include "ckdl.h"
#include "kdl/emitter.h"
#include "kdl/value.h"
#include "ruby/internal/core/rdata.h"
#include "ruby/internal/intern/object.h"

VALUE rb_mCKDL;
VALUE rb_cParseError;
VALUE rb_cParser;
VALUE rb_eParseError;
VALUE rb_cEmitter;
VALUE rb_eEmitError;

VALUE rb_mKDL;
VALUE rb_cDocument;
VALUE rb_cNode;
VALUE rb_cValue;
VALUE rb_cValueString;
VALUE rb_cValueInt;
VALUE rb_cValueFloat;
VALUE rb_cValueBool;
VALUE rb_cValueNull;
VALUE rb_KDLNull;

VALUE rb_cBigDecimal;

ID id_new;
ID id_name;
ID id_type;
ID id_value;
ID id_args;
ID id_props;
ID id_children;
ID id_as_type;
ID id_bigdecimal;

typedef struct s_parser {
    kdl_parser *parser;
} Parser;

void free_parser(Parser *parser) {
    if (parser->parser) {
        kdl_destroy_parser(parser->parser);
        parser->parser = NULL;
    }
    free(parser);
}

static const rb_data_type_t parser_type = {
    .wrap_struct_name = "kdl_parser",
    .function = {
        .dfree = RBIMPL_DATA_FUNC(free_parser),
        .dsize = 0
    },
    .flags = RUBY_TYPED_FREE_IMMEDIATELY
};

VALUE parser_alloc(VALUE self) {
    Parser *parser = malloc(sizeof(Parser));
    parser->parser = NULL;
    return TypedData_Wrap_Struct(self, &parser_type, parser);
}

VALUE rb_ckdl_parser_create_string_parser(VALUE self, VALUE string, VALUE version) {
    Parser *parser;
    TypedData_Get_Struct(self, Parser, &parser_type, parser);

    kdl_owned_string str;
    str.data = rb_string_value_ptr(&string);
    str.len = rb_str_strlen(string);

    parser->parser = kdl_create_string_parser(kdl_borrow_str(&str), NUM2INT(version));

    return Qnil;
}

size_t ckdl_read_io(void *user_data, char *buf, size_t bufsize) {
    VALUE io = (VALUE)user_data;
    // TODO: figure out how to read from IO?
    return 0;
}

VALUE rb_ckdl_parser_create_stream_parser(VALUE self, VALUE io, VALUE version) {
    Parser *parser;
    TypedData_Get_Struct(self, Parser, &parser_type, parser);

    parser->parser = kdl_create_stream_parser(ckdl_read_io, (void*)io, NUM2INT(version));

    return Qnil;
}

VALUE ckdl_str(kdl_str const *str) {
    if (!str->data) return Qnil;
    return rb_utf8_str_new(str->data, str->len);
}

kdl_str ckdl_kstr(VALUE str) {
    kdl_str kstr;
    kstr.data = rb_string_value_ptr(&str);
    kstr.len = RSTRING_LEN(str);
    return kstr;
}

kdl_number ckdl_int(VALUE val) {
    kdl_number num;
    if (FIXNUM_P(val)) {
        num.type = KDL_NUMBER_TYPE_INTEGER;
        num.integer = rb_num2ll(val);
    } else {
        num.type = KDL_NUMBER_TYPE_STRING_ENCODED;
        num.string = ckdl_kstr(rb_big2str(val, 10));
    }
    return num;
}

kdl_number ckdl_dbl(VALUE val) {
    kdl_number num;
    if (rb_obj_is_kind_of(val, rb_cBigDecimal)) {
        num.type = KDL_NUMBER_TYPE_STRING_ENCODED;
        num.string = ckdl_kstr(rb_String(val));
    } else {
        num.type = KDL_NUMBER_TYPE_FLOATING_POINT;
        num.floating_point = rb_num2dbl(val);
    }
    return num;
}

kdl_value *ckdl_kval(VALUE value) {
    VALUE type = rb_funcall(value, id_type, 0);
    VALUE val = rb_funcall(value, id_value, 0);
    kdl_value *kval = malloc(sizeof(kdl_value));
    kval->type_annotation = ckdl_kstr(type);
    if (rb_obj_is_kind_of(value, rb_cValueString)) {
        kval->type = KDL_TYPE_STRING;
        kval->string = ckdl_kstr(rb_funcall(value, id_value, 0));
    } else if (rb_obj_is_kind_of(value, rb_cValueInt)) {
        kval->type = KDL_TYPE_NUMBER;
        kval->number = ckdl_int(val);
    } else if (rb_obj_is_kind_of(value, rb_cValueFloat)) {
        kval->type = KDL_TYPE_NUMBER;
        kval->number = ckdl_dbl(val);
    } else if (rb_obj_is_kind_of(value, rb_cValueBool)) {
        kval->type = KDL_TYPE_BOOLEAN;
        kval->boolean = RTEST(val);
    } else if (rb_obj_is_kind_of(value, rb_cValueNull)) {
        kval->type = KDL_TYPE_NULL;
    } else {
        rb_raise(rb_eEmitError, "unable to convert value");
    }
    return kval;
}

NORETURN(void ckdl_raise_parse_error(kdl_event_data *data));

void ckdl_raise_parse_error(kdl_event_data *data) {
    if (data->event == KDL_EVENT_PARSE_ERROR) {
        rb_raise(rb_eParseError, "parse error");
    } else {
        rb_raise(rb_eParseError, "unexpected parse event (could be a bug in libkdl)");
    }
}

VALUE ckdl_bignum(const kdl_number *num) {
    VALUE str = ckdl_str(&num->string);
    VALUE result = rb_str_to_inum(str, 0, FALSE);
    if (result == INT2FIX(0)) {
        result = rb_funcall(rb_mKernel, id_bigdecimal, 1, str);
        return rb_funcall(rb_cValueFloat, id_new, 1, result);
    }
    return rb_funcall(rb_cValueInt, id_new, 1, result);
}

VALUE ckdl_number(const kdl_number *num) {
    switch (num->type) {
        case KDL_NUMBER_TYPE_INTEGER:
            return rb_funcall(rb_cValueInt, id_new, 1, ULL2NUM(num->integer));
        case KDL_NUMBER_TYPE_FLOATING_POINT:
            return rb_funcall(rb_cValueFloat, id_new, 1, DBL2NUM(num->floating_point));
        case KDL_NUMBER_TYPE_STRING_ENCODED:
            return ckdl_bignum(num);
    }
}

VALUE ckdl_value(const kdl_value *value) {
    VALUE kdl_value;

    switch (value->type) {
    case KDL_TYPE_NULL:
        kdl_value = rb_KDLNull;
    break;
    case KDL_TYPE_BOOLEAN:
        kdl_value = rb_funcall(rb_cValueBool, id_new, 1, value->boolean ? Qtrue : Qfalse);
    break;
    case KDL_TYPE_NUMBER:
        kdl_value = ckdl_number(&value->number);
    break;
    case KDL_TYPE_STRING:
        kdl_value = rb_funcall(rb_cValueString, id_new, 1, ckdl_str(&value->string));
    break;
    }

    return rb_funcall(kdl_value, id_as_type, 1, ckdl_str(&value->type_annotation));
}

kdl_event_data *ckdl_push_nodes(kdl_parser *parser, kdl_event_data *data, VALUE nodes);

void ckdl_push_node(kdl_parser *parser, kdl_event_data *data, VALUE nodes) {
    VALUE name = ckdl_str(&data->name);
    VALUE type = ckdl_str(&data->value.type_annotation);
    VALUE args = rb_ary_new();
    VALUE props = rb_hash_new();
    VALUE children = rb_ary_new();
    data = kdl_parser_next_event(parser);
    while (1) {
        switch (data->event) {
            case KDL_EVENT_ARGUMENT:
                rb_ary_push(args, ckdl_value(&data->value));
                data = kdl_parser_next_event(parser);
            break;
            case KDL_EVENT_PROPERTY:
                rb_hash_aset(props, ckdl_str(&data->name), ckdl_value(&data->value));
                data = kdl_parser_next_event(parser);
            break;
            case KDL_EVENT_START_NODE:
                data = ckdl_push_nodes(parser, data, children);
            break;
            case KDL_EVENT_END_NODE:
                {
                    VALUE *new_args = malloc(sizeof(VALUE) * 2);
                    new_args[0] = name;
                    new_args[1] = rb_hash_new_capa(1);
                    rb_hash_aset(new_args[1], ID2SYM(id_type), type);
                    rb_hash_aset(new_args[1], ID2SYM(id_args), args);
                    rb_hash_aset(new_args[1], ID2SYM(id_props), props);
                    rb_hash_aset(new_args[1], ID2SYM(id_children), children);
                    rb_ary_push(nodes, rb_funcallv_kw(rb_cNode, id_new, 2, new_args, RB_PASS_KEYWORDS));
                    free(new_args);
                }
                return;
            default:
                ckdl_raise_parse_error(data);
        }
    }
}

kdl_event_data *ckdl_push_nodes(kdl_parser *parser, kdl_event_data *data, VALUE nodes) {
    while (1) {
        switch (data->event) {
            case KDL_EVENT_START_NODE:
                ckdl_push_node(parser, data, nodes);
            break;
            case KDL_EVENT_PARSE_ERROR:
                ckdl_raise_parse_error(data);
            case KDL_EVENT_EOF:
            case KDL_EVENT_END_NODE:
                return data;
            default:
                ckdl_raise_parse_error(data);
        }
        data = kdl_parser_next_event(parser);
    }
}

VALUE rb_ckdl_parser_parse(VALUE self) {
    Parser *parser;
    TypedData_Get_Struct(self, Parser, &parser_type, parser);

    VALUE nodes = rb_ary_new();

    kdl_event_data *data = ckdl_push_nodes(parser->parser, kdl_parser_next_event(parser->parser), nodes);
    if (data->event != KDL_EVENT_EOF) {
        ckdl_raise_parse_error(data);
    }

    return rb_funcall(rb_cDocument, id_new, 1, nodes);
}

typedef struct s_emitter {
    kdl_emitter *emitter;
    int stream;
} Emitter;

void free_emitter(Emitter *emitter) {
    if (emitter->emitter) {
        kdl_destroy_emitter(emitter->emitter);
        emitter->emitter = NULL;
    }
    free(emitter);
}

static const rb_data_type_t emitter_type = {
    .wrap_struct_name = "kdl_emitter",
    .function = {
        .dfree = RBIMPL_DATA_FUNC(free_emitter),
        .dsize = 0
    },
    .flags = RUBY_TYPED_FREE_IMMEDIATELY
};

VALUE emitter_alloc(VALUE self) {
    Emitter *emitter = malloc(sizeof(Emitter));
    emitter->emitter = NULL;
    return TypedData_Wrap_Struct(self, &emitter_type, emitter);
}

VALUE ckdl_emitter_buffer(Emitter *emitter) {
    if (emitter->stream) return Qnil;
    kdl_str buffer = kdl_get_emitter_buffer(emitter->emitter);
    return ckdl_str(&buffer);
}

void ckdl_emit_arg(kdl_emitter *emitter, VALUE value) {
    kdl_value *kval = ckdl_kval(value);
    kdl_emit_arg(emitter, kval);
    free(kval);
}

int ckdl_emit_property(VALUE key, VALUE value, VALUE arg) {
    kdl_emitter *emitter = (kdl_emitter*)arg;

    kdl_value *kval = ckdl_kval(value);
    kdl_emit_property(emitter, ckdl_kstr(key), kval);
    free(kval);

    return ST_CONTINUE;
}

void ckdl_emit_node(kdl_emitter *emitter, VALUE node) {
    VALUE name = rb_funcall(node, id_name, 0);
    VALUE type = rb_funcall(node, id_type, 0);

    if (NIL_P(type)) {
        kdl_emit_node(emitter, ckdl_kstr(name));
    } else {
        kdl_emit_node_with_type(emitter, ckdl_kstr(type), ckdl_kstr(name));
    }

    VALUE args = rb_funcall(node, id_args, 0);
    long count = RARRAY_LEN(args);
    for (int i = 0; i < count; i++) {
        ckdl_emit_arg(emitter, rb_ary_entry(args, i));
    }

    VALUE props = rb_funcall(node, id_props, 0);
    rb_hash_foreach(props, ckdl_emit_property, (VALUE)emitter);

}

VALUE rb_ckdl_emitter_emit_document(VALUE self, VALUE document) {
    Emitter *emitter;
    TypedData_Get_Struct(self, Emitter, &emitter_type, emitter);

    VALUE nodes = rb_funcall(document, rb_intern("nodes"), 0);
    long count = RARRAY_LEN(nodes);
    for (int i = 0; i < count; i++) {
        VALUE node = rb_ary_entry(nodes, i);
        ckdl_emit_node(emitter->emitter, node);
    }

    kdl_emit_end(emitter->emitter);

    return ckdl_emitter_buffer(emitter);
}

VALUE rb_ckdl_emitter_emit_node(VALUE self, VALUE node) {
    Emitter *emitter;
    TypedData_Get_Struct(self, Emitter, &emitter_type, emitter);

    return ckdl_emitter_buffer(emitter);
}

VALUE rb_ckdl_emitter_emit_value(VALUE self, VALUE value) {
    Emitter *emitter;
    TypedData_Get_Struct(self, Emitter, &emitter_type, emitter);

    return ckdl_emitter_buffer(emitter);
}


void ckdl_set_float_mode(VALUE float_mode, kdl_emitter_options *options) {
    VALUE opt;
    opt = rb_hash_aref(float_mode, ID2SYM(rb_intern("always_write_decimal_point")));
    if (!NIL_P(opt)) options->float_mode.always_write_decimal_point = RTEST(opt);

    opt = rb_hash_aref(float_mode, ID2SYM(rb_intern("always_write_decimal_point_or_exponent")));
    if (!NIL_P(opt)) options->float_mode.always_write_decimal_point_or_exponent = RTEST(opt);

    opt = rb_hash_aref(float_mode, ID2SYM(rb_intern("capital_e")));
    if (!NIL_P(opt)) options->float_mode.capital_e = RTEST(opt);

    opt = rb_hash_aref(float_mode, ID2SYM(rb_intern("exponent_plus")));
    if (!NIL_P(opt)) options->float_mode.exponent_plus = RTEST(opt);

    opt = rb_hash_aref(float_mode, ID2SYM(rb_intern("plus")));
    if (!NIL_P(opt)) options->float_mode.plus = RTEST(opt);

    opt = rb_hash_aref(float_mode, ID2SYM(rb_intern("min_exponent")));
    if (!NIL_P(opt)) options->float_mode.min_exponent = NUM2INT(opt);
}

void ckdl_set_emitter_options(VALUE version, VALUE escape_mode, VALUE identifier_mode, VALUE float_mode, kdl_emitter_options *options) {
    options->version = NUM2INT(version);
    if (!NIL_P(escape_mode)) options->escape_mode = NUM2INT(escape_mode);
    if (!NIL_P(identifier_mode)) options->identifier_mode = NUM2INT(identifier_mode);
    if (!NIL_P(float_mode)) ckdl_set_float_mode(float_mode, options);
}

VALUE rb_ckdl_emitter_create_buffering_emitter(VALUE self, VALUE version, VALUE escape_mode, VALUE identifier_mode, VALUE float_mode) {
    Emitter *emitter;
    TypedData_Get_Struct(self, Emitter, &emitter_type, emitter);
    emitter->stream = FALSE;

    kdl_emitter_options options = KDL_DEFAULT_EMITTER_OPTIONS;
    ckdl_set_emitter_options(version, escape_mode, identifier_mode, float_mode, &options);

    emitter->emitter = kdl_create_buffering_emitter(&options);

    return Qnil;
}

size_t ckdl_write_io(void *user_data, const char *data, size_t nbytes) {
    VALUE io = (VALUE)user_data;
    // TODO: figure out how to write to IO?
    return 0;
}

VALUE rb_ckdl_emitter_create_stream_emitter(VALUE self, VALUE io, VALUE version, VALUE escape_mode, VALUE identifier_mode, VALUE float_mode) {
    Emitter *emitter;
    TypedData_Get_Struct(self, Emitter, &emitter_type, emitter);
    emitter->stream = TRUE;

    kdl_emitter_options options = KDL_DEFAULT_EMITTER_OPTIONS;
    ckdl_set_emitter_options(version, escape_mode, identifier_mode, float_mode, &options);

    emitter->emitter = kdl_create_stream_emitter(ckdl_write_io, (void*)io, &options);

    return Qnil;
}

RUBY_FUNC_EXPORTED void
Init_libckdl(void)
{
    id_new = rb_intern("new");
    id_name = rb_intern("name");
    id_type = rb_intern("type");
    id_value = rb_intern("value");
    id_args = rb_intern("arguments");
    id_props = rb_intern("properties");
    id_children = rb_intern("children");
    id_as_type = rb_intern("as_type");
    id_bigdecimal = rb_intern("BigDecimal");

    rb_mKDL = rb_const_get(rb_cObject, rb_intern("KDL"));
    rb_cDocument = rb_const_get(rb_mKDL, rb_intern("Document"));
    rb_cNode = rb_const_get(rb_mKDL, rb_intern("Node"));
    rb_cValue = rb_const_get(rb_mKDL, rb_intern("Value"));
    rb_cValueString = rb_const_get(rb_cValue, rb_intern("String"));
    rb_cValueInt = rb_const_get(rb_cValue, rb_intern("Int"));
    rb_cValueFloat = rb_const_get(rb_cValue, rb_intern("Float"));
    rb_cValueBool = rb_const_get(rb_cValue, rb_intern("Boolean"));
    rb_cValueNull = rb_const_get(rb_cValue, rb_intern("NullImpl"));
    rb_KDLNull = rb_const_get(rb_cValue, rb_intern("Null"));

    rb_cBigDecimal = rb_const_get(rb_cObject, rb_intern("BigDecimal"));

    rb_mCKDL = rb_define_module("CKDL");

    rb_eParseError = rb_define_class_under(rb_mCKDL, "ParseError", rb_eStandardError);
    rb_eEmitError = rb_define_class_under(rb_mCKDL, "EmitError", rb_eStandardError);

    // kdl_type
    rb_define_const(rb_mCKDL, "TYPE_NULL", INT2NUM(KDL_TYPE_NULL));
    rb_define_const(rb_mCKDL, "TYPE_BOOLEAN", INT2NUM(KDL_TYPE_BOOLEAN));
    rb_define_const(rb_mCKDL, "TYPE_NUMBER", INT2NUM(KDL_TYPE_NUMBER));
    rb_define_const(rb_mCKDL, "TYPE_STRING", INT2NUM(KDL_TYPE_STRING));

    // kdl_number_type
    rb_define_const(rb_mCKDL, "NUMBER_TYPE_INTEGER", INT2NUM(KDL_NUMBER_TYPE_INTEGER));
    rb_define_const(rb_mCKDL, "NUMBER_TYPE_FLOATING_POINT", INT2NUM(KDL_NUMBER_TYPE_FLOATING_POINT));
    rb_define_const(rb_mCKDL, "NUMBER_TYPE_STRING_ENCODED", INT2NUM(KDL_NUMBER_TYPE_STRING_ENCODED));

    // kdl_event
    rb_define_const(rb_mCKDL, "EVENT_EOF", INT2NUM(KDL_EVENT_EOF));
    rb_define_const(rb_mCKDL, "EVENT_PARSE_ERROR", INT2NUM(KDL_EVENT_PARSE_ERROR));
    rb_define_const(rb_mCKDL, "EVENT_START_NODE", INT2NUM(KDL_EVENT_START_NODE));
    rb_define_const(rb_mCKDL, "EVENT_END_NODE", INT2NUM(KDL_EVENT_END_NODE));
    rb_define_const(rb_mCKDL, "EVENT_ARGUMENT", INT2NUM(KDL_EVENT_ARGUMENT));
    rb_define_const(rb_mCKDL, "EVENT_PROPERTY", INT2NUM(KDL_EVENT_PROPERTY));
    rb_define_const(rb_mCKDL, "EVENT_COMMENT", INT2NUM(KDL_EVENT_COMMENT));

    rb_cParser = rb_define_class_under(rb_mCKDL, "Parser", rb_cObject);
    rb_define_alloc_func(rb_cParser, parser_alloc);

    // kdl_parse_option
    rb_define_const(rb_cParser, "DEFAULTS", INT2NUM(KDL_DEFAULTS));
    rb_define_const(rb_cParser, "EMIT_COMMENTS", INT2NUM(KDL_EMIT_COMMENTS));
    rb_define_const(rb_cParser, "READ_VERSION_1", INT2NUM(KDL_READ_VERSION_1));
    rb_define_const(rb_cParser, "READ_VERSION_2", INT2NUM(KDL_READ_VERSION_2));
    rb_define_const(rb_cParser, "DETECT_VERSION", INT2NUM(KDL_DETECT_VERSION));

    rb_define_method(rb_cParser, "parse", rb_ckdl_parser_parse, 0);

    rb_define_private_method(rb_cParser, "create_string_parser", rb_ckdl_parser_create_string_parser, 2);
    rb_define_private_method(rb_cParser, "create_stream_parser", rb_ckdl_parser_create_stream_parser, 2);

    rb_cEmitter = rb_define_class_under(rb_mCKDL, "Emitter", rb_cObject);
    rb_define_alloc_func(rb_cEmitter, emitter_alloc);

    // kdl_escape_mode
    rb_define_const(rb_cEmitter, "ESCAPE_MINIMAL", INT2NUM(KDL_ESCAPE_MINIMAL));
    rb_define_const(rb_cEmitter, "ESCAPE_CONTROL", INT2NUM(KDL_ESCAPE_CONTROL));
    rb_define_const(rb_cEmitter, "ESCAPE_NEWLINE", INT2NUM(KDL_ESCAPE_NEWLINE));
    rb_define_const(rb_cEmitter, "ESCAPE_TAB", INT2NUM(KDL_ESCAPE_TAB));
    rb_define_const(rb_cEmitter, "ESCAPE_ASCII_MODE", INT2NUM(KDL_ESCAPE_ASCII_MODE));
    rb_define_const(rb_cEmitter, "ESCAPE_DEFAULT", INT2NUM(KDL_ESCAPE_DEFAULT));

    // kdl_version
    rb_define_const(rb_cEmitter, "VERSION_1", INT2NUM(KDL_VERSION_1));
    rb_define_const(rb_cEmitter, "VERSION_2", INT2NUM(KDL_VERSION_2));

    // kdl_identifier_emission_mod
    rb_define_const(rb_cEmitter, "PREFER_BARE_IDENTIFIERS", INT2NUM(KDL_PREFER_BARE_IDENTIFIERS));
    rb_define_const(rb_cEmitter, "QUOTE_ALL_IDENTIFIERS", INT2NUM(KDL_QUOTE_ALL_IDENTIFIERS));
    rb_define_const(rb_cEmitter, "ASCII_IDENTIFIERS", INT2NUM(KDL_ASCII_IDENTIFIERS));

    rb_define_method(rb_cEmitter, "emit_document", rb_ckdl_emitter_emit_document, 1);
    rb_define_method(rb_cEmitter, "emit_node", rb_ckdl_emitter_emit_node, 1);
    rb_define_method(rb_cEmitter, "emit_value", rb_ckdl_emitter_emit_value, 1);

    rb_define_private_method(rb_cEmitter, "create_buffering_emitter", rb_ckdl_emitter_create_buffering_emitter, 4);
    rb_define_private_method(rb_cEmitter, "create_stream_emitter", rb_ckdl_emitter_create_stream_emitter, 5);
}
