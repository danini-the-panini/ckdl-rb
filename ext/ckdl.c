#include "ckdl.h"

VALUE rb_mCKDL;
VALUE rb_cParseError;
VALUE rb_cParser;
VALUE rb_eParseError;

VALUE rb_mKDL;
VALUE rb_cDocument;
VALUE rb_cNode;
VALUE rb_cValue;
VALUE rb_cValueString;
VALUE rb_cValueInt;
VALUE rb_cValueFloat;
VALUE rb_cValueBool;
VALUE rb_KDLNull;

ID id_new;
ID id_name;
ID id_type;
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

VALUE ckdl_str(kdl_str const *str) {
    if (!str->data) return Qnil;
    return rb_utf8_str_new(str->data, str->len);
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

size_t rb_ckdl_read_io(void *user_data, char *buf, size_t bufsize) {
    VALUE io = (VALUE)user_data;
    // TODO: figure out how to read from IO?
    return 0;
}

VALUE rb_ckdl_parser_create_stream_parser(VALUE self, VALUE io, VALUE version) {
    Parser *parser;
    TypedData_Get_Struct(self, Parser, &parser_type, parser);

    parser->parser = kdl_create_stream_parser(rb_ckdl_read_io, (void*)io, NUM2INT(version));

    return Qnil;
}

RUBY_FUNC_EXPORTED void
Init_libckdl(void)
{
    id_new = rb_intern("new");
    id_name = rb_intern("name");
    id_type = rb_intern("type");
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
    rb_KDLNull = rb_const_get(rb_cValue, rb_intern("Null"));

    rb_mCKDL = rb_define_module("CKDL");

    rb_eParseError = rb_define_class_under(rb_mCKDL, "ParseError", rb_eStandardError);

    // kdl_escape_mode
    rb_define_const(rb_mCKDL, "ESCAPE_MINIMAL", INT2NUM(KDL_ESCAPE_MINIMAL));
    rb_define_const(rb_mCKDL, "ESCAPE_CONTROL", INT2NUM(KDL_ESCAPE_CONTROL));
    rb_define_const(rb_mCKDL, "ESCAPE_NEWLINE", INT2NUM(KDL_ESCAPE_NEWLINE));
    rb_define_const(rb_mCKDL, "ESCAPE_TAB", INT2NUM(KDL_ESCAPE_TAB));
    rb_define_const(rb_mCKDL, "ESCAPE_ASCII_MODE", INT2NUM(KDL_ESCAPE_ASCII_MODE));
    rb_define_const(rb_mCKDL, "ESCAPE_DEFAULT", INT2NUM(KDL_ESCAPE_DEFAULT));

    // kdl_version
    rb_define_const(rb_mCKDL, "VERSION_1", INT2NUM(KDL_VERSION_1));
    rb_define_const(rb_mCKDL, "VERSION_2", INT2NUM(KDL_VERSION_2));

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
}
