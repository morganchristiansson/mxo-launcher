#!/usr/bin/env ruby

input_file = 'r2-vtables.txt'

# Read the entire file
content = File.read(input_file)

# Split by table boundaries - find each ### Table X header
tables = content.split(/^Vtable Found at /m)

puts "Found #{tables.length} tables (including empty first element)"

tables.shift
tables.each_with_index do |table_content, idx|
  # next if idx == 0 && table_content.strip.empty?  # Skip empty first element
  table_content.strip!

  # The actual content starts after "### Table X\n\n"
  lines = table_content.lines

  # Parse the pointer address from this line
  # if match = lines[0].match(/- \*\*Address\*\*: 0x([0-9a-fA-F]+)/)
    # require 'pry' ; binding.pry
    # ptr_addr = "0x#{match[1]}"
  ptr_addr = lines.shift.strip

    # Create filename with address
    filename = "#{ptr_addr}.md"

    # Write the content
    File.write(filename, lines.join)
    puts "[#{idx + 1}] Generated #{filename}"
  # end
end

puts "\nDone!"
