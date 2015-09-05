#!/usr/bin/env ruby

presets_dir = "/home/shlomi/progs/freecell/trunk/fc-solve/presets/soft-threads/meta-moves/auto-gen"

start_board = 11983
fcs_output = IO.popen("./freecell-solver-range-parallel-solve #{start_board} 32000 1 --read-from-file 4,#{presets_dir}/nameless-preset.bash --scans-synergy none")

expected_iters = File.open("#{presets_dir}/nameless-simulation.txt", "r")

junk_line = fcs_output.readline()
total_iters = 0

prev_to_start_board = start_board - 1
while ((wait_line = expected_iters.readline()) && \
       (wait_line !~ /^#{prev_to_start_board}:/))
    # Do nothing - we're waiting to the end.
end

while (out_line = fcs_output.readline()) do
    out_line.chomp!
    exp_line = expected_iters.readline()
    exp_line.chomp!
    # puts "{{#{out_line}}}->{{#{exp_line}}}"

    if exp_line =~ /:(\d+)\z/
        total_iters += $1.to_i
    else
        raise "Incorrect expected format at line '#{exp_line}'"
    end

    if (out_line =~ /\(total_num_iters=(\d+)\)/)
        if $1.to_i != total_iters
            raise "Wrong iteration count at <<#{exp_line}>> and <<#{out_line}>>"
        end
    else
        raise "Incorrect output format at line '#{out_line}'"
    end
end
