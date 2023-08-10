//////////////////////////////////////////////////////////////////////////////////
// Company: UQAC
// Engineer: SHACHA
// 
// Create Date: 07/07/2023 02:55:30 PM
// Design Name: 
// Module Name: as_rv32i_fetch
// Project Name: 
// Target Devices: 
// Tool Versions: 
// Description: Pipeline Stage 1 retrieves instruction from the memory [FETCH STAGE]
/*
    The as_rv32i_fetch module is largely responsible for retrieving instructions from memory and preparing them for the pipeline's decode stage. The module is in charge of directing the pipeline, managing the Programme Counter (PC), and fetching instructions. The following are the main functions of the as_rv32i_fetch module:
 - Programme Counter (PC) administration: The module keeps track of a Programme Counter (PC), which stores the address of the current instruction in memory. The PC is initialised at the reset vector address (given by the PC_RESET parameter), and it is incremented or updated dependent on instruction flow control, such as branches, jumps, or traps.
 - Instruction fetching: Based on the current PC value, the module retrieves the instruction from memory. When the fetch stage (ce) is activated, it sends a request for a new instruction (o_stb_inst) and waits for an acknowledgment (i_ack_inst) from memory. The retrieved instruction (i_inst) is then routed to the pipeline (o_inst).
 - Pipeline management: The as_rv32i_fetch module manages the pipeline by managing the following stage's clock enable (o_ce) signals. When the next stages are stalled (i_stall), a requested instruction has not yet been acknowledged, or there is no request for a new instruction, it can stall the fetch stage (stall_fetch). Furthermore, when the PC has to be changed, it can generate pipeline bubbles by disabling clock enable signals for the next stages, ensuring no instructions are executed during this time.
 - PC control: The module updates the PC based on control signals received from previous stages of the pipeline. When handling traps, it can update the PC with a new address (i_writeback_next_pc) or the address of a taken branch or jump (i_alu_next_pc). During this process, the fetch stage might be halted to prevent instructions from being executed in the pipeline.
 - Dealing with stalls and flushes: The as_rv32i_fetch module can halt the fetch stage under various scenarios and save the current PC and instruction values. When the stall situation has been overcome, it can return to the previously saved values and continue fetching instructions. When necessary, the module can flush the fetch stage (i_flush), deactivating the clock enable signal for the next stage and thus clearing any outstanding instructions.
*/ 
// Dependencies: 
// 
// Revision:
// Revision 0.01 - File Created
// Additional Comments:
// 
//////////////////////////////////////////////////////////////////////////////////


`timescale 1ns / 1ps
`default_nettype none
`include "as_rv32i_header.vh"

module as_rv32i_fetch #(parameter PC_RESET = 32'h00_00_00_00) (
    input wire i_clk,i_rst_n,
    output reg[31:0] o_iaddr,   // instruction memory address
    output reg[31:0] o_pc,      // PC value of current instruction 
    input wire[31:0] i_inst,    // retrieved instruction from Memory
    output reg[31:0] o_inst,    // instruction sent to pipeline
    output wire o_stb_inst,     // request for instruction
    input wire i_ack_inst,      // ack (high when bus contains new instruction)

    /// PC Control ///
    input wire i_writeback_change_pc,       // high when PC needs to change when going to trap or returning from trap
    input wire[31:0] i_writeback_next_pc,   // next PC due to trap
    input wire i_alu_change_pc,             // high when PC needs to change for taken branches and jumps
    input wire[31:0] i_alu_next_pc,         // next PC due to branch or jump

    /// Pipeline Control ///
    output reg o_ce,    // output clk enable for pipeline stalling of next stage
    input wire i_stall, // stall logic for whole pipeline
    input wire i_flush  // flush this stage
);

    reg[31:0] iaddr_d, prev_pc, stalled_inst, stalled_pc;
    reg ce, ce_d;
    reg stall_fetch;
    reg stall_q;

    /* Stall this 1st stage when:
        - Next stages are stalled
        - New Instruction request without ack
        - No request
    */
    wire stall_bit = stall_fetch || i_stall || (o_stb_inst && !i_ack_inst) || !o_stb_inst; 
    assign o_stb_inst = ce; // request for new instruction if this stage is enabled                                                               
    
    //clock enable (ce) logic for 1st fetch stage
    always @(posedge i_clk, negedge i_rst_n) begin
         if(!i_rst_n) ce <= 0;
         else if((i_alu_change_pc || i_writeback_change_pc) && !(i_stall || stall_fetch)) ce <= 0; // When changing pc, create a pipeline bubble so that the next stages are disabled and the instructions already inside the pipeline are not executed.
         else ce <= 1;                                                  
     end



    always @(posedge i_clk, negedge i_rst_n) begin
        if(!i_rst_n) begin
            o_ce <= 0;
            o_iaddr <= PC_RESET;
            prev_pc <= PC_RESET;
            stalled_inst <= 0;
            o_pc <= 0;
        end
        else begin 
            if((ce && !stall_bit) || (stall_bit && !o_ce && ce) || i_writeback_change_pc) begin // Update registers only if this stage is enabled and next stages are not stalled
                o_iaddr <= iaddr_d;
                o_pc <= stall_q? stalled_pc:prev_pc;
                o_inst <= stall_q? stalled_inst:i_inst;
            end
            if(i_flush && !stall_bit) begin // flush this stage(only when not stalled) so that clock-enable of next stage is disabled at next clock cycle
                o_ce <= 0;
            end
            else if(!stall_bit) begin // clock-enable will change only when not stalled
                o_ce <= ce_d;
            end

            // If this stage is stalled but the next stage is not, disable the following stage's clock enablement at the next clock cycle (pipeline bubble).
            else if(stall_bit && !i_stall) o_ce <= 0; 


            stall_q <= i_stall || stall_fetch; // Raise stall when any of 5 stages is stalled

            // Before stalling, save both the instruction and the PC so that we may return to these values when we need to exit the stall. 
            if(stall_bit && !stall_q) begin
                stalled_pc <= prev_pc; 
                stalled_inst <= i_inst; 
            end
            prev_pc <= o_iaddr; // This is the first delay to align the PC to the pipeline
        end
    end
    // Logic for PC and pipeline clock_enable control
    always @* begin
        iaddr_d = 0;
        ce_d = 0;
        stall_fetch = i_stall; // When changing PC, stall when retrieving instructions, then do a pipeline bubble to disable the ce of the following stage.
        if(i_writeback_change_pc) begin
            iaddr_d = i_writeback_next_pc;
            ce_d = 0;
        end
        else if(i_alu_change_pc) begin
            iaddr_d  = i_alu_next_pc;
            ce_d = 0;
        end
        else begin
            iaddr_d = o_iaddr + 32'd4;
            ce_d = ce;
        end
    end
    
endmodule






