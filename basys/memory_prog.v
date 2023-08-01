module main_memory #(
    parameter MEMORY_DEPTH = 1024
) (
    // ... (other input and output ports)
    input wire i_uart_rx,        // UART receiver data input
    input wire i_uart_rx_ready,  // UART receiver ready signal
    input wire i_uart_rx_valid,  // UART receiver data valid signal
    // ... (other input and output ports)
);

    // ... (other internal signals and declarations)

    // Memory programming states
    parameter IDLE = 2'b00;
    parameter START = 2'b01;
    parameter PROGRAM = 2'b10;

    reg [1:0] mem_prog_state;

    always @(posedge i_clk) begin
        if (i_rst) begin
            mem_prog_state <= IDLE;
        end else begin
            case (mem_prog_state)
                IDLE:
                    // Wait for a memory programming start command
                    if (i_uart_rx_valid && i_uart_rx_ready && (i_uart_rx == 8'hA5)) begin
                        mem_prog_state <= START;
                        o_ack_inst <= 0;
                        o_ack_data <= 0;
                        o_inst_out <= 0;
                    end
                START:
                    // Receive the address and data to be programmed
                    if (i_uart_rx_valid && i_uart_rx_ready) begin
                        mem_prog_state <= PROGRAM;
                        mem_prog_addr <= i_uart_rx;
                    end
                PROGRAM:
                    // Program the data into memory and wait for next address
                    if (i_uart_rx_valid && i_uart_rx_ready) begin
                        memory_regfile[mem_prog_addr] <= i_uart_rx;
                        mem_prog_state <= START;
                    end
            endcase
        end
    end

    // ... (other logic and memory read/write operations)

endmodule
