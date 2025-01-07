# frozen_string_literal: true

require "mkmf"
require "pathname"
require "fileutils"
require "open-uri"
require "uri"

# Makes all symbols private by default to avoid unintended conflict
# with other gems. To explicitly export symbols you can use RUBY_FUNC_EXPORTED
# selectively, or entirely remove this flag.
append_cflags("-fvisibility=hidden")

BASE_DIR = Pathname(__FILE__).dirname.parent.parent.expand_path
$LOAD_PATH.unshift(BASE_DIR.to_s)

LIBKDL_VERSION = "1.0"

def cmake_exe
  ENV["CMAKE"] ||= find_executable("cmake")
end

def local_libkdl_base_dir
  File.join(BASE_DIR, "ext", "vendor")
end

def local_libkdl_install_dir
  File.expand_path(File.join(local_libkdl_base_dir, "local"))
end

def install_libkdl
  FileUtils.mkdir_p(local_libkdl_base_dir)

  build_libkdl
end

def download(url)
  message("downloading %s...", url)
  base_name = File.basename(url)
  if File.exist?(base_name)
    message(" skip (use downloaded file)\n")
  else
    options = {}
    proxy_env = ENV["http_proxy"]
    if proxy_env
      proxy_url = URI.parse(proxy_env)
      if proxy_url.user
        options[:proxy_http_basic_authentication] = [
          proxy_url,
          proxy_url.user,
          proxy_url.password,
        ]
      end
    end
    URI.open(url, "rb", *options) do |input|
      File.open(base_name, "wb") do |output|
        while (buffer = input.read(1024))
          output.print(buffer)
        end
      end
    end
    message(" done\n")
  end
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

def build_and_install(install_dir)
  FileUtils.mkdir("build")
  Dir.chdir("build") do
    run_command("configuring...", "#{cmake_exe} -DBUILD_KDLPP=OFF -DBUILD_TESTS=OFF ..")
    run_command("building...", "#{cmake_exe} --build . --config Release")
    run_command("installing...", "#{cmake_exe} --install . --prefix #{install_dir}")
  end
end

def build_libkdl_from_git
  libkdl_repo_dir = "libkdl-main"
  message("removing old cloned repository...")
  FileUtils.rm_rf(libkdl_repo_dir)
  message(" done\n")

  repository_url = "https://github.com/tjol/ckdl.git"
  run_command("cloning...",
    "git clone --recursive --depth 1 #{repository_url} #{libkdl_repo_dir}")

  Dir.chdir(libkdl_repo_dir) do
    build_and_install(local_libkdl_install_dir)
  end

  message("removing cloned repository...")
  FileUtils.rm_rf(libkdl_repo_dir)
  message(" done\n")
end

def build_libkdl_from_tar_gz
  tar_gz = "#{LIBKDL_VERSION}.tar.gz"
  url = "https://github.com/tjol/ckdl/archive/refs/tags/#{tar_gz}"
  libkdl_source_dir = "libkdl-#{LIBKDL_VERSION}"

  download(url)

  FileUtils.rm_rf(libkdl_source_dir)
  FileUtils.mkdir_p(libkdl_source_dir)

  message("extracting...")
  if xsystem("tar xfz #{tar_gz} -C #{libkdl_source_dir} --strip-components=1")
    message(" done\n")
  else
    message(" failed\n")
    exit(false)
  end

  Dir.chdir(libkdl_source_dir) do
    build_and_install(local_libkdl_install_dir)
  end

  message("removing source...")
  FileUtils.rm_rf(libkdl_source_dir)
  message(" done\n")

  message("removing source archive...")
  FileUtils.rm_rf(tar_gz)
  message(" done\n")
end

def build_libkdl
  if LIBKDL_VERSION == 'main'
    build_libkdl_from_git
  else
    build_libkdl_from_tar_gz
  end
end

build_libkdl unless File.exist?(local_libkdl_install_dir)
append_cflags("-I#{local_libkdl_install_dir}/include")
append_ldflags("-L#{local_libkdl_install_dir}/lib -lkdl")

create_makefile("ckdl/ckdl")
