require 'test_helper'

class V2Test < Minitest::Test
  TEST_CASES_DIR = File.join(__dir__, '../ext/libkdl/tests/test_documents/upstream/2.0.0')
  INPUTS_DIR = File.join(TEST_CASES_DIR, 'input')
  EXPECTED_DIR = File.join(TEST_CASES_DIR, 'expected_kdl')

  Dir.glob(File.join(INPUTS_DIR, '*.kdl')).each do |input_path|
    input_name = File.basename(input_path, '.kdl')
    expected_path = File.join(EXPECTED_DIR, "#{input_name}.kdl")
    if File.exist?(expected_path)
      define_method "test_#{input_name}_matches_expected_output" do
        expected = File.read(expected_path, encoding: Encoding::UTF_8).chomp
        assert_equal expected, ::KDL.parse(File.read(input_path, encoding: Encoding::UTF_8), version: 2).to_s.chomp
      end
    else
      define_method "test_#{input_name}_does_not_parse" do
        assert_raises { ::KDL.parse(File.read(input_path, encoding: Encoding::UTF_8), version: 2) }
      end
    end
  end
end
