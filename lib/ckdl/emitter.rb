# frozen_string_literal: true

module CKDL
  class Emitter
    def initialize(version: 2, escape_mode: nil, identifier_mode: nil, float_mode: {}, io: nil)
      @float_mode = {
        capital_e: true,
        always_write_decimal_point: true,
        exponent_plus: true,
        **float_mode
      }
      @io = io

      version_option = case version
      when 1 then VERSION_1
      when 2 then VERSION_2
      else raise KDL::UnsupportedVersionError.new("Unsupported version '#{version}'", version)
      end

      if io
        create_stream_emitter(io, version_option, escape_mode, identifier_mode, @float_mode);
      else
        create_buffering_emitter(version_option, escape_mode, identifier_mode, @float_mode);
      end
    end

    module Document
      def to_s
        Emitter.new(version:).emit_document(self)
      end
    end
    KDL::Document.prepend(Document)

    module Node
      def to_s
        Emitter.new(version:).emit_node(self)
      end
    end
    class KDL::Node
      alias old_to_s to_s
      def inspect(level = 0)
        old_to_s(level, :inspect)
      end
    end
    KDL::Node.prepend(Node)

    module Value
      def to_s
        Emitter.new(version:).emit_value(self)
      end
    end
    KDL::Value.prepend(Value)
    KDL::V1::Value::Methods.prepend(Value)
  end
end
