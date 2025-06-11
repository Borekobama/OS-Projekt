function odd_even_simulation()
    % This simulates the odd-even sorting algorithm
    % Creates a gif automatically
    
    % Setup gif file
    output_file = 'odd_even_sort.gif';
    current_frame = 0;
    
    % Configuration settings
    num_processes = 4;
    total_elements = 100;
    initial_wait = 2.0;      % Wait at the beginning
    presort_wait = 2.0;
    swap_wait = 0.35;
    round_wait = 0.8;
    
    % Create some random numbers to sort
    rng(42); % seed for reproducibility
    unsorted_data = randi([1, 100], 1, total_elements);
    
    % Figure out how many elements each process gets
    base_chunk_size = floor(total_elements / num_processes);
    leftover = mod(total_elements, num_processes);
    
    % Split up the data between processes
    process_chunks = cell(num_processes, 1);
    current_position = 1;
    
    for proc = 1:num_processes
        % First few processes get an extra element if there's remainder
        if proc <= leftover
            this_chunk_size = base_chunk_size + 1;
        else
            this_chunk_size = base_chunk_size;
        end
        
        process_chunks{proc} = unsorted_data(current_position:current_position + this_chunk_size - 1);
        current_position = current_position + this_chunk_size;
    end
    
    % Convert to matrix format (easier to work with)
    max_chunk_length = base_chunk_size + (leftover > 0);
    process_data = zeros(num_processes, max_chunk_length);
    
    for proc = 1:num_processes
        chunk_len = length(process_chunks{proc});
        process_data(proc, 1:chunk_len) = process_chunks{proc};
        % Mark empty spots with NaN
        if chunk_len < max_chunk_length
            process_data(proc, chunk_len+1:end) = NaN;
        end
    end
    
    % Create the figure window
    fig_handle = figure('Position', [100, 100, 1200, 700], 'Color', 'white');
    set(fig_handle, 'Resize', 'off');
    
    % Show initial state
    draw_current_state(process_data, num_processes, 'Distribution', [], []);
    save_animation_frame(initial_wait);
    
    % STEP 1: Each process sorts locally
    fprintf('STEP 1: Local sorting within each process\n');
    for proc = 1:num_processes
        valid_values = process_data(proc, ~isnan(process_data(proc, :)));
        sorted_values = sort(valid_values);
        process_data(proc, 1:length(sorted_values)) = sorted_values;
    end
    draw_current_state(process_data, num_processes, 'Presort', [], []);
    save_animation_frame(presort_wait);
    
    % STEP 2: Start odd-even transposition
    still_swapping = true;
    round_number = 0;
    
    while still_swapping
        round_number = round_number + 1;
        still_swapping = false;
        
        % ODD PHASE - odd processes swap with right neighbor
        fprintf('Round %d - Odd phase\n', round_number);
        swapped_processes = [];
        
        for proc = 1:2:num_processes-1
            [process_data, did_swap] = swap_neighbors(process_data, proc, proc+1);
            if did_swap
                swapped_processes = [swapped_processes, proc, proc+1];
            end
            still_swapping = still_swapping || did_swap;
        end
        
        % Draw odd phase
        draw_current_state(process_data, num_processes, ...
            'Odd-Even Sorting', swapped_processes, 'odd');
        save_animation_frame(swap_wait);
        
        % EVEN PHASE - even processes swap with right neighbor
        fprintf('Round %d - Even phase\n', round_number);
        swapped_processes = [];
        
        for proc = 2:2:num_processes-1
            [process_data, did_swap] = swap_neighbors(process_data, proc, proc+1);
            if did_swap
                swapped_processes = [swapped_processes, proc, proc+1];
            end
            still_swapping = still_swapping || did_swap;
        end
        
        if still_swapping
            % Draw even phase
            draw_current_state(process_data, num_processes, ...
                'Odd-Even Sorting', swapped_processes, 'even');
            save_animation_frame(swap_wait);
        end
        
        % Show round summary
        if ~still_swapping
            draw_current_state(process_data, num_processes, ...
                sprintf('Sorted after %d Rounds', round_number), [], []);
        else
            draw_current_state(process_data, num_processes, ...
                'Odd-Even Sorting', [], []);
        end
        save_animation_frame(round_wait);
    end
    
    % All done!
    fprintf('Sorting finished in %d rounds\n', round_number);
    fprintf('Animation saved as %s\n', output_file);
    
    % Helper function to save frames
    function save_animation_frame(wait_time)
        pause(wait_time);
        
        frame_data = getframe(gcf);
        rgb_image = frame2im(frame_data);
        [indexed_image, color_map] = rgb2ind(rgb_image, 256);
        
        current_frame = current_frame + 1;
        if current_frame == 1
            % First frame - create new file
            imwrite(indexed_image, color_map, output_file, 'gif', ...
                'Loopcount', inf, 'DelayTime', wait_time);
        else
            % Add to existing file
            imwrite(indexed_image, color_map, output_file, 'gif', ...
                'WriteMode', 'append', 'DelayTime', wait_time);
        end
    end
end

function draw_current_state(data, num_procs, title_str, highlighted_procs, phase)
    % Draw the current state of the sorting process
    clf;
    hold on;
    
    % Make sure background is white
    set(gcf, 'Color', 'white');
    set(gca, 'Color', 'white');
    
    % Draw data for each process
    x_position = 0;
    for proc = 1:num_procs
        % Get the actual data (ignore NaN values)
        actual_data = data(proc, ~isnan(data(proc, :)));
        if isempty(actual_data)
            continue;
        end
        
        x_coords = x_position + (1:length(actual_data));
        y_coords = actual_data;
        
        % Figure out which elements to highlight in red
        highlight_these = false(size(y_coords));
        if ismember(proc, highlighted_procs)
            if strcmp(phase, 'odd') && mod(proc, 2) == 1
                % Odd process in odd phase - highlight rightmost element
                highlight_these(end) = true;
            elseif strcmp(phase, 'odd') && mod(proc, 2) == 0
                % Even process in odd phase - highlight leftmost element
                highlight_these(1) = true;
            elseif strcmp(phase, 'even') && mod(proc, 2) == 0
                % Even process in even phase - highlight rightmost element
                highlight_these(end) = true;
            elseif strcmp(phase, 'even') && mod(proc, 2) == 1
                % Odd process in even phase - highlight leftmost element
                highlight_these(1) = true;
            end
        end
        
        % Draw normal points in black
        normal_points = ~highlight_these;
        if any(normal_points)
            scatter(x_coords(normal_points), y_coords(normal_points), 60, 'k', 'filled', ...
                'MarkerEdgeColor', 'black', 'LineWidth', 0.5);
        end
        
        % Draw swapped points in red
        if any(highlight_these)
            scatter(x_coords(highlight_these), y_coords(highlight_these), 100, 'r', 'filled', ...
                'MarkerEdgeColor', 'black', 'LineWidth', 2);
        end
        
        % Add divider lines between processes
        if proc < num_procs
            divider_x = x_position + length(actual_data) + 0.5;
            xline(divider_x, '--', 'Color', [0.5 0.5 0.5], 'LineWidth', 2);
        end
        
        % Label each process
        label_x = x_position + length(actual_data)/2;
        if proc == 1
            text(label_x, -8, 'Coordinator (0)', 'HorizontalAlignment', 'center', ...
                'FontSize', 12, 'FontWeight', 'bold', 'Color', 'k');
        else
            text(label_x, -8, sprintf('Worker %d', proc-1), 'HorizontalAlignment', 'center', ...
                'FontSize', 12, 'FontWeight', 'bold', 'Color', 'k');
        end
        
        x_position = x_position + length(actual_data);
    end
    
    % Set up axes
    xlabel('Element Position', 'FontSize', 14);
    ylabel('Value', 'FontSize', 14);
    title(title_str, 'FontSize', 18, 'FontWeight', 'bold');
    
    xlim([0, x_position + 1]);
    ylim([-15, 110]);
    grid on;
    set(gca, 'FontSize', 12);
    
    drawnow;
end

function [data, swapped_flag] = swap_neighbors(data, proc1, proc2)
    % Check if neighbors need to swap their boundary elements
    swapped_flag = false;
    
    % Get the actual data for both processes
    proc1_values = data(proc1, ~isnan(data(proc1, :)));
    proc2_values = data(proc2, ~isnan(data(proc2, :)));
    
    if isempty(proc1_values) || isempty(proc2_values)
        return;
    end
    
    % Check boundary values
    proc1_rightmost = proc1_values(end);
    proc2_leftmost = proc2_values(1);
    
    % Swap if needed
    if proc1_rightmost > proc2_leftmost
        % Do the swap
        proc1_values(end) = proc2_leftmost;
        proc2_values(1) = proc1_rightmost;
        
        % Re-sort locally after swap
        proc1_values = sort(proc1_values);
        proc2_values = sort(proc2_values);
        
        % Put back in matrix
        data(proc1, 1:length(proc1_values)) = proc1_values;
        data(proc2, 1:length(proc2_values)) = proc2_values;
        
        swapped_flag = true;
    end
end