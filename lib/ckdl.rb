# frozen_string_literal: true

require "kdl"

require_relative "ckdl/version"
require_relative "ckdl/libckdl"
require_relative "ckdl/parser"
require_relative "ckdl/emitter"

module CKDL
  def parse(input, version: KDL.default_version, output_version: KDL.default_output_version, **options)
    version_option = case version
    when 1 then CKDL::Parser::READ_VERSION_1
    when 2 then CKDL::Parser::READ_VERSION_2
    when nil then CKDL::Parser::DETECT_VERSION
    else raise KDL::UnsupportedVersionError.new("Unsupported version '#{version}'", version)
    end
    CKDL::Parser.new(input, version: version_option, output_version: output_version || version || 2, **options).parse
  end

  def load_file(filespec, **options)
    File.open(filespec, 'r:BOM|UTF-8') do |file|
      load_stream(io, **options)
    end
  end

  def load_stream(io, **options)
    parse(io, **options)
  end

  def auto_parse(input, **options)
    parse(input, version: nil, **options)
  end

  extend self
end

KDL.singleton_class.module_eval { prepend CKDL }
