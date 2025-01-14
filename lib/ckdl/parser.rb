# frozen_string_literal: true

module CKDL
  class Parser
    def initialize(input, version: DETECT_VERSION, output_version: 2, **options)
      @input = input

      case input
      when String then create_string_parser(input, version, output_version)
      when IO then create_stream_parser(input, version, output_version)
      else raise ArgumentError, "input must be a String or IO"
      end
    end
  end
end
