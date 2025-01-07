# frozen_string_literal: true

module CKDL
  class Parser
    def initialize(input, version: KDL.default_version, **options)
      case input
      when String then create_string_parser(input, version)
      when IO then create_stream_parser(input, version)
      else raise ArgumentError, "input must be a String or IO"
      end
    end
  end
end
