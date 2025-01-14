# frozen_string_literal: true

require "test_helper"

class TestCKDL < Minitest::Test
  def test_that_it_has_a_version_number
    refute_nil ::CKDL::VERSION
  end

  def test_output_version
    assert_equal 2, CKDL.parse("node 1").version
    assert_equal 1, CKDL.parse("node 1", output_version: 1).version
  end
end
