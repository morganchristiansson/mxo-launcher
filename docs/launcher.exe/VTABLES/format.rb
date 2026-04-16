#!/usr/bin/env ruby
# frozen_string_literal: true

require 'pathname'

class VTableMarkdownUpdater
  def initialize(filepath)
    @path = Pathname(filepath)
    @filename = @path.basename.to_s
    @lines = @path.readlines.map(&:chomp)
    @base_addr = extract_base_address
  end

  def run
    return unless @base_addr

    changed = false

    changed |= ensure_heading
    changed |= convert_plain_to_minimal_table_if_needed
    changed |= normalize_offsets_in_table_if_present

    if changed
      @path.write(@lines.join("\n") + "\n")
      puts "Updated: #{@filename}"
    else
      puts "No changes: #{@filename}"
    end
  end

  private

  def extract_base_address
    return nil unless @filename =~ /^0x[0-9a-f]{8}\.md$/i
    @filename[2..9].to_i(16)
  end

  def heading_line
    "# 0x#{'%08x' % @base_addr}"
  end

  def ensure_heading
    return false if @lines.empty?

    first = @lines.first.strip
    return false if first == heading_line

    if first =~ /^# 0x[0-9a-f]{8}/i
      @lines[0] = heading_line
      return true
    end

    # Insert at top
    @lines.unshift(heading_line)
    @lines.insert(1, "") if @lines.size > 1 && @lines[1].strip != ""
    true
  end

  # ────────────────────────────────────────────────
  # Convert plain list → minimal 2-column table
  # Only runs if file looks like plain list and has no markdown table
  # ────────────────────────────────────────────────
  def convert_plain_to_minimal_table_if_needed

    if @lines.any? { |l| l.strip.start_with?('|') && l.include?('---') }
      return false
    end

    entries = []
    non_empty_lines = 0

    @lines.each_with_index do |line, idx|
      stripped = line.strip
      next if stripped.empty?

      non_empty_lines += 1

      md = stripped.match(/^0x([0-9a-f]{8})\s*:\s*(.+)$/i)
      if md
        entries << [md[1].to_i(16), md[2].strip]
      end
    end

    return false if entries.empty?

    table_lines = [
      "| Offset | Function Name |",
      "|--------|---------------|"
    ]

    entries.each_with_index do |(addr, name), i|
      offset = addr - @base_addr
      next unless offset >= 0 && (offset % 4).zero?   # skip invalid offsets

      offset_str = format('+0x%02x', offset)
      # Right-pad offset column, left-align name
      table_lines << "| #{offset_str.ljust(6)} | #{name} |"
    end

    # Build new content
    new_lines = []

    # Keep original heading if it exists and starts with # 0x...
    if @lines.first&.strip&.start_with?('# 0x')
      new_lines << @lines.first
    else
      new_lines << heading_line
    end

    new_lines << "" unless new_lines.last.empty?   # one blank line after heading
    new_lines.concat(table_lines)

    @lines.replace(new_lines)
    true
  end

  # ────────────────────────────────────────────────
  # Normalize first column in existing table (0x... → +0xxx)
  # Preserves trailing | and spacing
  # ────────────────────────────────────────────────
  def normalize_offsets_in_table_if_present
    in_table = false
    changed = false

    @lines.each_with_index do |line, i|
      stripped = line.strip

      if stripped.start_with?('|') && stripped =~ /Offset|Address/i
        in_table = true
        next
      end

      next unless in_table

      if stripped.empty? || !stripped.start_with?('|')
        in_table = false
        next
      end

      next if stripped =~ /^\|[-\s:|]+$/

      parts = line.split('|', -1)
      next if parts.size < 3

      cell_raw = parts[1]
      cell = cell_raw.strip

      next unless cell =~ /^0x[0-9a-f]{8}$/i

      begin
        val = cell[2..].to_i(16)
        offset = val - @base_addr
        next unless offset >= 0 && offset % 4 == 0

        new_offset = format('+0x%02x', offset)

        new_cell = cell_raw.sub(cell, new_offset)
        parts[1] = new_cell

        @lines[i] = parts.join('|')
        changed = true
      rescue
        # silent fail
      end
    end

    changed
  end
end

# ─── Main ────────────────────────────────────────
if ARGV.empty?
  abort "Usage:\n  ruby #{$0} file.md\n  ruby #{$0} *.md"
end

ARGV.each do |pat|
  Pathname.glob(pat).each do |path|
    next unless path.file? && path.extname.downcase == '.md'
    VTableMarkdownUpdater.new(path).run
  end
end
