/**
 * @file
 *  
 * @author   UQAC | SHACHA
 * 
 * @date     07/07/2023 01:34:30 PM
 * 
 * @module as_rv32i_basereg
 * @brief Regfile Controller for the 32 integer base registers
 * 
 * Dependencies: 
 * 
 * Revision:
 * Revision 0.01 - File Created 
 * Additional Comments:
 * 
 */

/**
 * @brief Top-level module for controlling a register file.
 *
 * This module handles the read and write operations for a 32-integer base register file.
 * It includes support for synchronous read and write operations, and it avoids writing to the
 * reserved base register 0.
 */
`timescale 1ns / 1ps
`default_nettype none

module as_rv32i_basereg
    (
        input wire i_clk,            ///< Clock signal
        input wire i_ce_read,        ///< Clock enable for reading from basereg [STAGE 2]
        input wire[4:0] i_rs1_addr,  ///< Source register 1 address
        input wire[4:0] i_rs2_addr,  ///< Source register 2 address
        input wire[4:0] i_rd_addr,   ///< Destination register address
        input wire[31:0] i_rd,       ///< Data to be written to destination register
        input wire i_wr,             ///< Write enable
        output wire[31:0] o_rs1,     ///< Source register 1 value
        output wire[31:0] o_rs2      ///< Source register 2 value
    );
    
    reg[4:0] rs1_addr_q, rs2_addr_q;
    reg[31:0] base_regfile[31:1];   ///< Base register file (with base_regfile[0] hardwired to zero)
    wire write_to_basereg;
    
    /**
     * @brief Sequential logic for read and write operations.
     *
     * This block handles synchronous read and write operations based on clock signals.
     * It reads source register addresses and data to be written, and it updates the base register file.
     */
    always @(posedge i_clk) begin
        if(write_to_basereg) begin ///< Only write to register if stage 5 is previously enabled (output of stage 5[WRITEBACK] is registered so delayed by 1 clk)
           base_regfile[i_rd_addr] <= i_rd; ///< Synchronous write
        end
        if(i_ce_read) begin ///< Only read the register if stage 2 is enabled [DECODE]
            rs1_addr_q <= i_rs1_addr; ///< Synchronous read
            rs2_addr_q <= i_rs2_addr; ///< Synchronous read
        end
    end

    /**
     * @brief Control signal for write operation.
     *
     * Determines whether a write operation should be performed on the base register file.
     */
    assign write_to_basereg = i_wr && i_rd_addr!=0;

    /**
     * @brief Output for source register 1 value.
     *
     * Determines the output value for source register 1 based on the read address.
     */
    assign o_rs1 = rs1_addr_q==0? 0: base_regfile[rs1_addr_q]; 

    /**
     * @brief Output for source register 2 value.
     *
     * Determines the output value for source register 2 based on the read address.
     */
    assign o_rs2 = rs2_addr_q==0? 0: base_regfile[rs2_addr_q]; 
    
endmodule
