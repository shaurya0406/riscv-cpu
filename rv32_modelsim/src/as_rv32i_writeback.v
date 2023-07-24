//////////////////////////////////////////////////////////////////////////////////
// Company: UQAC
// Engineer: SHACHA
// 
// Create Date: 09/07/2023 01:48:15 PM
// Design Name: 
// Module Name: as_rv32i_writeback
// Project Name: 
// Target Devices: 
// Tool Versions: 
// Description: Pipeline Stage 5, logic controller for the next PC and rd value [WRITEBACK STAGE]
/* 
   The pipelined processor's writeback stage is handled by the as_rv32i_writeback module. This stage is in charge of determining the next programme counter (PC) value, writing data back to the destination register, and dealing with trap-related operations (interrupts and exceptions). Furthermore, the module is in charge of pipeline control, such as halting and flushing prior stages. 
   The as_rv32i_writeback module's key features are as follows:
 - Determining the next programme counter (PC) value and updating the o_next_pc register: If the module detects an interrupt or exception, it sets the PC to the trap address (i_trap_address) and asserts the o_change_pc signal. When the processor exits a trap, the module sets the PC to the return address (i_return_address) and asserts the o_change_pc signal. In normal operation, the preceding stage's PC value (i_pc) is passed across.
 - Handling destination register writeback: Based on the instruction's opcode and funct3 fields, the module writes data back to the target register: If the instruction is a load operation, memory data (i_data_load) is written back. The CSR value (i_csr_out) is written back if the instruction is a CSR write operation. In other circumstances, the data is computed and written back at the ALU stage (i_rd). The i_wr_rd input and the current pipeline control state (i_ce and o_stall) are used to determine the o_wr_rd signal. The i_rd_addr input is used to send over the destination register address (o_rd_addr).
 - Trap-handler control: The module handles interrupts and exceptions by checking the i_go_to_trap and i_return_from_trap input signals. When the processor enters a trap, it sets the o_next_pc register to the trap address (i_trap_address) and flushes the pipeline. When the processor exits a trap, it sets the o_next_pc register to the return address (i_return_address) and flushes the pipeline.
 - Pipeline control: When necessary, the module can stall the pipeline by asserting the o_stall signal. Based on the condition of the pipeline and trap-related activities, it can additionally flush the current and prior stages by asserting the o_flush signal.
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

module as_rv32i_writeback (
    input wire[2:0] i_funct3, //function type 
    input wire[31:0] i_data_load, //data to be loaded to base reg
    input wire[31:0] i_csr_out, //CSR value to be loaded to basereg
    input wire i_opcode_load,
    input wire i_opcode_system, 

    /// Basereg Control ///
    input wire i_wr_rd,         //write rd to basereg if enabled (from previous stage)
    output reg o_wr_rd,         //write rd to the base reg if enabled
    input wire[4:0] i_rd_addr,  //address for destination register (from previous stage)
    output reg[4:0] o_rd_addr,  //address for destination register
    input wire[31:0] i_rd,      //value to be written back to destination register (from previous stage)
    output reg[31:0] o_rd,      //value to be written back to destination register

    /// PC Control ///
    input wire[31:0] i_pc,      //pc value (from previous stage)
    output reg[31:0] o_next_pc, //new pc value
    output reg o_change_pc,     //high if PC needs to jump

    /// Trap-Handler ///
    input wire i_go_to_trap,            //high before going to trap (if exception/interrupt detected)
    input wire i_return_from_trap,      //high before returning from trap (via mret)
    input wire[31:0] i_return_address,  //mepc CSR
    input wire[31:0] i_trap_address,    //mtvec CSR

    /// Pipeline Control ///
    input wire i_ce,    //input clk enable for pipeline stalling of this stage
    output reg o_stall, //informs pipeline to stall
    output reg o_flush  //flush previous stages
);
//
    //determine next value of pc and o_rd
    always @* begin
        o_stall = 0; //stall when this stage needs wait time
        o_flush = 0; //flush this stage along with previous stages when changing PC
        o_wr_rd = i_wr_rd && i_ce && !o_stall;
        o_rd_addr = i_rd_addr;
        o_rd = 0;
        o_next_pc = 0;
        o_change_pc = 0;

        if(i_go_to_trap) begin
            o_change_pc = 1;            //change PC only when ce of this stage is high (o_change_pc is valid)
            o_next_pc = i_trap_address; //interrupt or exception detected so go to trap address (mtvec value)
            o_flush = i_ce;
            o_wr_rd = 0;
        end
        
        else if(i_return_from_trap) begin
            o_change_pc = 1;                //change PC only when ce of this stage is high (o_change_pc is valid)
             o_next_pc = i_return_address;  //return from trap via mret (mepc value)
             o_flush = i_ce;
             o_wr_rd = 0;
        end
        
        else begin //normal operation
            if(i_opcode_load) o_rd = i_data_load; //load data from memory to basereg
            else if(i_opcode_system && i_funct3!=0) begin //CSR write
                o_rd = i_csr_out; 
            end
            else o_rd = i_rd; //rd value is already computed at ALU stage
        end
        
    end
endmodule
