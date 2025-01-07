# frozen_string_literal: true

require "bundler/gem_tasks"
require "minitest/test_task"

Minitest::TestTask.create

require "rake/extensiontask"

task build: :compile

GEMSPEC = Gem::Specification.load("ckdl.gemspec")

Rake::ExtensionTask.new("ckdl", GEMSPEC) do |ext|
  ext.lib_dir = "lib/ckdl"
end

task :clobber_vendor do
  FileUtils.rm_rf(File.join(__dir__, "ext/vendor"))
end

task clobber: :clobber_vendor

task default: %i[clobber compile test]
