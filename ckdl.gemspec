# frozen_string_literal: true

require_relative "lib/ckdl/version"

Gem::Specification.new do |spec|
  spec.name = "ckdl"
  spec.version = CKDL::VERSION
  spec.authors = ["Danielle Smith"]
  spec.email = ["code@danini.dev"]

  spec.summary = "ckdl bindings"
  spec.description = "KDL Document Language using bindings to the ckdl library"
  spec.homepage = "https://kdl.dev"
  spec.license = "MIT"
  spec.required_ruby_version = ">= 3.1.0"

  spec.metadata["homepage_uri"] = spec.homepage
  spec.metadata["source_code_uri"] = "https://github.com/danini-the-panini/ckdl-rb"
  spec.metadata["changelog_uri"] = "https://github.com/danini-the-panini/ckdl-rb/releases"

  spec.files = Dir.glob([
    "LICENSE.txt",
    "README.md",
    "ext/*.c",
    "ext/*.h",
    "ext/CMakeLists.txt",
    "ext/libkdl/CMakeLists.txt",
    "ext/libkdl/COPYING",
    "ext/libkdl/include/**/*",
    "ext/libkdl/src/**/*",
    "ext/libkdl/bindings/cpp/*",
    "lib/**/*.rb",
  ], File::FNM_DOTMATCH).select { |path| File.file?(path) }
  spec.bindir = "exe"
  spec.executables = spec.files.grep(%r{\Aexe/}) { |f| File.basename(f) }
  spec.require_paths = ["lib"]
  spec.extensions = ["ext/extconf.rb"]
  spec.rdoc_options << "--exclude" << "ext/"

  spec.add_dependency "kdl", "~> 2.0"
end
