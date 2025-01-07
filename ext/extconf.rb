# frozen_string_literal: true

require "mkmf"
require "tempfile"

lib = File.expand_path("../lib", __dir__)
$LOAD_PATH.unshift(lib) unless $LOAD_PATH.include?(lib)
require "ckdl/version"

def which(name, extra_locations = [])
  ENV.fetch("PATH", "")
     .split(File::PATH_SEPARATOR)
     .prepend(*extra_locations)
     .select { |path| File.directory?(path) }
     .map { |path| [path, name].join(File::SEPARATOR) + RbConfig::CONFIG["EXEEXT"] }
     .find { |file| File.executable?(file) }
end

def check_version(name, extra_locations = [])
  executable = which(name, extra_locations)
  puts "-- check #{name} (#{executable})"
  version = nil
  IO.popen([executable, "--version"]) do |io|
    version = io.read.split("\n").first
  end
  [executable, version]
end

def version_at_least?(version, other)
  major, minor = version.split('.')
  cmajor, cminor = version.split('.')
  return true if major > cmajor
  return false if major < cmajor
  minor >= cminor
end

cmake_extra_locations = []
if RUBY_PLATFORM =~ /mswin|mingw/
  cmake_extra_locations = [
    'C:\Program Files\CMake\bin',
    'C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin',
    'C:\Program Files\Microsoft Visual Studio\2019\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin',
  ]
  local_app_data = ENV.fetch("LOCALAPPDATA", "#{ENV.fetch("HOME")}\\AppData\\Local")
  if File.directory?(local_app_data)
    cmake_extra_locations.unshift("#{local_app_data}\\CMake\\bin")
  end
end
cmake, version = check_version("cmake", cmake_extra_locations)
unless version_at_least?(version[/cmake version (\d+\.\d+)/, 1], '3.8')
  cmake, version = check_version("cmake3")
end
abort "ERROR: CMake is required to build kdl extension." unless cmake
puts "-- #{version}, #{cmake}"

def sys(*cmd)
  puts "-- #{Dir.pwd}"
  puts "-- #{cmd.join(' ')}"
  system(*cmd) || abort("failed to execute command: #{$?.inspect}\n#{cmd.join(' ')}")
end

def run_command(start_message, command)
  message(start_message)
  if xsystem(command)
    message(" done\n")
  else
    message(" failed\n")
    exit(false)
  end
end

build_type = ENV["DEBUG"] ? "Debug" : "Release"
cmake_flags = [
  "-DCMAKE_BUILD_TYPE=#{build_type}",
  "-DRUBY_HDR_DIR=#{RbConfig::CONFIG['rubyhdrdir']}",
  "-DRUBY_ARCH_HDR_DIR=#{RbConfig::CONFIG['rubyarchhdrdir']}",
  "-DRUBY_LIBRARY_DIR=#{RbConfig::CONFIG['libdir']}",
  "-DRUBY_LIBRUBYARG=#{RbConfig::CONFIG['LIBRUBYARG_SHARED']}",
  "-DBUILD_KDLPP=OFF",
  "-DBUILD_TESTS=OFF"
]

cc = ENV["KDL_CC"]
cxx = ENV["KDL_CXX"]
ar = ENV["KDL_AR"]

if RbConfig::CONFIG["target_os"] =~ /mingw/
  require "ruby_installer/runtime"
  RubyInstaller::Runtime.enable_dll_search_paths
  RubyInstaller::Runtime.enable_msys_apps
  cc = RbConfig::CONFIG["CC"]
  cxx = RbConfig::CONFIG["CXX"]
  cmake_flags << "-G Ninja"
  cmake_flags << "-DRUBY_LIBRUBY=#{File.basename(RbConfig::CONFIG["LIBRUBY_SO"], ".#{RbConfig::CONFIG["SOEXT"]}")}"
end

cmake_flags << "-DCMAKE_C_COMPILER=#{cc}" if cc
cmake_flags << "-DCMAKE_CXX_COMPILER=#{cxx}" if cxx
cmake_flags << "-DCMAKE_AR=#{ar}" if ar

project_path = File.expand_path(File.join(__dir__))

build_dir = ENV['KDL_EXT_BUILD_DIR'] ||
            File.join(Dir.tmpdir, "kdl-#{build_type}-#{RUBY_VERSION}-#{RUBY_PATCHLEVEL}-#{RUBY_PLATFORM}-#{CKDL::VERSION}")
FileUtils.rm_rf(build_dir, verbose: true) unless ENV['KDL_PRESERVE_BUILD_DIR']
FileUtils.mkdir_p(build_dir, verbose: true)
Dir.chdir(build_dir) do
  puts "-- build #{build_type} extension #{CKDL::VERSION} for ruby #{RUBY_VERSION}-#{RUBY_PATCHLEVEL}-#{RUBY_PLATFORM}"
  sys(cmake, *cmake_flags, "-B#{build_dir}", "-S#{project_path}")
  number_of_jobs = (ENV["KDL_NUMBER_OF_JOBS"] || 4).to_s
  sys(cmake, "--build", build_dir, "--parallel", number_of_jobs,  "--verbose")
end
extension_name = "libckdl.#{RbConfig::CONFIG['SOEXT'] || RbConfig::CONFIG['DLEXT']}"
extension_path = File.expand_path(File.join(build_dir, extension_name))
abort "ERROR: failed to build extension in #{extension_path}" unless File.file?(extension_path)
extension_name.gsub!(/\.dylib/, '.bundle')
if RbConfig::CONFIG["target_os"] =~ /mingw/
  extension_name.gsub!(/\.dll$/, '.so')
end
install_path = File.expand_path(File.join(__dir__, "..", "lib", "ckdl", extension_name))
puts "-- copy extension to #{install_path}"
FileUtils.cp(extension_path, install_path, verbose: true)
ext_directory = File.expand_path(__dir__)
create_makefile("libckdl")
if ENV["KDL_REMOVE_EXT_DIRECTORY"]
  puts "-- KDL_REMOVE_EXT_DIRECTORY is set, remove #{ext_directory}"
  exceptions = %w[. .. extconf.rb]
  Dir
    .glob("#{ext_directory}/*", File::FNM_DOTMATCH)
    .reject { |path| exceptions.include?(File.basename(path)) || File.basename(path).start_with?(".gem") }
    .each do |entry|
    puts "-- remove #{entry}"
    FileUtils.rm_rf(entry, verbose: true)
  end
  File.truncate("#{ext_directory}/extconf.rb", 0)
  puts "-- truncate #{ext_directory}/extconf.rb"
end

File.write("#{ext_directory}/Makefile", <<~MAKEFILE)
  .PHONY: all clean install
  all:
  clean:
  install:
MAKEFILE
