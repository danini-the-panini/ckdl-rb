# frozen_string_literal: true

require "kdl"

require_relative "ckdl/version"
require_relative "ckdl/libckdl"
require_relative "ckdl/parser"

module KDL
  def self.parse(input, version: default_version, **options)
    version_option = case version
    when 1 then CKDL::Parser::READ_VERSION_1
    when 2 then CKDL::Parser::READ_VERSION_2
    when nil then CKDL::Parser::DETECT_VERSION
    else raise KDL::UnsupportedVersionError.new("Unsupported version '#{version}'", version)
    end
    CKDL::Parser.new(input, version: version_option, **options).parse
  end

  def self.load_file(filespec, **options)
    File.open(filespec, 'r:BOM|UTF-8') do |file|
      load_stream(io, **options)
    end
  end

  def self.load_stream(io, **options)
    parse(io, **options)
  end

  def self.auto_parse(input, **options)
    parse(input, version: nil, **options)
  end
end
