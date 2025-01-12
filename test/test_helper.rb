# frozen_string_literal: true

$LOAD_PATH.unshift File.expand_path("../lib", __dir__)
require "ckdl"

require "minitest/autorun"
require "minitest/reporters"
Minitest::Reporters.use!

Minitest::Assertions.diff = 'diff -u --color=always'
