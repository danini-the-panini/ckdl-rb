# frozen_string_literal: true

require "bundler/gem_tasks"
require "minitest/test_task"

Minitest::TestTask.create

desc "Compile binary extension"
task :compile do
  require 'tempfile'
  Dir.chdir(Dir.tmpdir) do # rubocop:disable ThreadSafety/DirChdir
    sh "ruby '#{File.join(__dir__, 'ext', 'extconf.rb')}'"
  end
end

task build: :compile

task default: %i[clobber compile test]
