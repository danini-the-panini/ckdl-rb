require 'test_helper'

class V1Test < Minitest::Test
  TEST_CASES_DIR = File.join(__dir__, '../ext/libkdl/tests/test_documents/upstream/1.0.0')
  INPUTS_DIR = File.join(TEST_CASES_DIR, 'input')
  EXPECTED_DIR = File.join(TEST_CASES_DIR, 'expected_kdl')

  EXCLUDE = %w[
    no_decimal_exponent
  ]

  Dir.glob(File.join(INPUTS_DIR, '*.kdl')).each do |input_path|
    input_name = File.basename(input_path, '.kdl')
    next if EXCLUDE.include?(input_name)

    expected_path = File.join(EXPECTED_DIR, "#{input_name}.kdl")
    if File.exist?(expected_path)
      define_method "test_#{input_name}_matches_expected_output" do
        expected = File.read(expected_path, encoding: Encoding::UTF_8).chomp
        assert_equal expected, ::KDL.load_file(input_path, version: 1).to_s.chomp
      end
    else
      define_method "test_#{input_name}_does_not_parse" do
        assert_raises { ::KDL.load_file(input_path, version: 1) }
      end
    end
  end
end
