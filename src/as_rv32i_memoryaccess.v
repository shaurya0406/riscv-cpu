//////////////////////////////////////////////////////////////////////////////////
// Company: UQAC
// Engineer: SHACHA
// 
// Create Date: 08/07/2023 01:15:29 PM
// Design Name: 
// Module Name: as_rv32i_memoryaccess
// Project Name: 
// Target Devices: 
// Tool Versions: 
// Description: Pipeline Stage 4 loads and stores data in memory [MEMORY ACCESS STAGE]
/* 
   The pipelined processor's memory access stage is handled by the as_rv32i_memoryaccess module. This module is largely responsible for data memory access for load and save instructions, as well as sending the required information to later pipeline stages. The module is in charge of creating data memory addresses, data to be stored, and write masks for various load/store processes, as well as handling pipeline stalls and flushes as needed. 
   The following are key features of the as_rv32i_memoryaccess module:
 - Handling addresses and data for load/store operations: The module generates the appropriate data memory address (o_y) from the incoming address (i_y) and saves it in the o_data_store register. It also selects the appropriate byte, halfword, or word data from the data memory input (i_din) based on the funct3 field of the instruction and stores it in the o_data_load register. The write mask (o_wr_mask) is generated based on the address and operation size (byte, halfword, or word). The mask determines which section of the data memory is written during store operations.
 - Register writeback control: Based on the input i_wr_rd signal, the module decides whether a destination register should be written (o_wr_rd). It sends the next stage the destination register address (o_rd_addr) and the data to be written (o_rd).
 - Data memory control: The module manages data memory read/write requests by sending out the o_stb_data signal, which represents a data memory access request. It also creates the o_wr_mem signal, which signals if a write operation on the data memory should be done.
 - Pipeline control: If the data memory access has not yet been acknowledged (i_ack_data) or there is a stall request from the ALU stage (i_stall_from_alu), the module can stall the pipeline by asserting the o_stall signal. It can also use the o_flush signal to flush the current and previous stages based on the input i_flush signal. Based on the stall and flush conditions, the module regulates the clock enable signals (o_ce) for the following step.
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

module as_rv32i_memoryaccess(
    input wire i_clk, i_rst_n,
    input wire[31:0] i_rs2,                 //data to be stored to memory is always i_rs2
    input wire[31:0] i_din,                 //data retrieve from memory 
    input wire[31:0] i_y,                   //y value from ALU (address of data to memory be stored or loaded)
    output reg [31:0] o_y,                  //y value used as data memory address 
    input wire[2:0] i_funct3,               //funct3 from previous stage
    output reg[2:0] o_funct3,               //funct3 (byte,halfword,word)
    input wire[`OPCODE_WIDTH-1:0] i_opcode, //determines if data_store will be to stored to data memory
    output reg[`OPCODE_WIDTH-1:0] o_opcode, //opcode type
    input wire[31:0] i_pc,                  //PC from previous stage
    output reg[31:0] o_pc,                  //PC value

    /// Basereg Control ///
    input wire i_wr_rd,         //write rd to base reg is enabled (from memoryaccess stage)
    output reg o_wr_rd,         //write rd to the base reg if enabled
    input wire[4:0] i_rd_addr,  //address for destination register (from previous stage)
    output reg[4:0] o_rd_addr,  //address for destination register
    input wire[31:0] i_rd,      //value to be written back to destination reg
    output reg[31:0] o_rd,      //value to be written back to destination register

    /// Data Memory Control ///
    output reg[31:0] o_data_store,  //data to be stored to memory (mask-aligned)
    output reg[31:0] o_data_load,   //data to be loaded to base reg (z-or-s extended) 
    output reg[3:0] o_wr_mask,      //write mask {byte3,byte2,byte1,byte0}
    output reg o_wr_mem,            //write to data memory if enabled
    output reg o_stb_data,          //request for read/write access to data memory
    input wire i_ack_data,          //ack by data memory (high when read data is ready or when write data is already written)

    /// Pipeline Control ///
    input wire i_stall_from_alu,    //stalls this stage when incoming instruction is a load/store
    input wire i_ce,                //input clk enable for pipeline stalling of this stage
    output reg o_ce,                //output clk enable for pipeline stalling of next stage
    input wire i_stall,             //informs this stage to stall
    output reg o_stall,             //informs pipeline to stall
    input wire i_flush,             //flush this stage
    output reg o_flush              //flush previous stages
);
    
    reg[31:0] data_store_d;         //data to be stored to memory
    reg[31:0] data_load_d;          //data to be loaded to basereg
    reg[3:0] wr_mask_d; 
    wire[1:0] addr_2 = i_y[1:0];    //last 2 bits of data memory address
    wire stall_bit = i_stall || o_stall;

    //register the outputs of this module
    always @(posedge i_clk, negedge i_rst_n) begin
        if(!i_rst_n) begin
            o_wr_rd <= 0;
            o_wr_mem <= 0;
            o_ce <= 0;
            o_stb_data <= 0;
        end
        else begin
            if(i_ce && !stall_bit) begin //update register only if this stage is enabled and not stalled
                o_rd_addr <= i_rd_addr;
                o_funct3 <= i_funct3;
                o_opcode <= i_opcode;
                o_pc <= i_pc;
                o_wr_rd <= i_wr_rd;
                o_rd <= i_rd;
                o_data_load <= data_load_d; 
                o_wr_mask <= 0;
                o_wr_mem <= 0; 
                o_stb_data <= 0;
            end
            else if(i_ce) begin
                //stb goes high when instruction is a load/store and when request is not already high (request lasts for 1 clk cycle only)
                o_stb_data <= (o_stb_data && stall_bit)? 0:i_opcode[`LOAD] || i_opcode[`STORE]; 
                o_wr_mask <= o_stb_data? 0:wr_mask_d;
                o_wr_mem <= o_stb_data? 0:i_opcode[`STORE]; 
            end

            //update this registers even on stall since this will be used for accessing the data memory while on stall
            o_y <= i_y; //data memory address
            o_data_store <= data_store_d;

            if(i_flush && !stall_bit) begin //flush this stage so clock-enable of next stage is disabled at next clock cycle
                o_ce <= 0;
                o_wr_mem <= 0;
            end
            else if(!stall_bit) begin //clock-enable will change only when not stalled
                o_ce <= i_ce;
            end
            //if this stage is stalled but next stage is not, disable clock enable of next stage at next clock cycle (pipeline bubble)
            else if(stall_bit && !i_stall) o_ce <= 0;         
        end

    end 

    //determine data to be loaded to basereg or stored to data memory 
    always @* begin
        //stall while data memory has not yet acknowledged i.e.wWrite data is not yet written or read data is not yet available. Don't stall when need to flush by next stage
        o_stall = ((i_stall_from_alu && i_ce && !i_ack_data) || i_stall) && !i_flush;         
        o_flush = i_flush; //flush this stage along with previous stages
        data_store_d = 0;
        data_load_d = 0;
        wr_mask_d = 0; 
           
        case(i_funct3[1:0]) 
            2'b00: begin //byte load/store
                    case(addr_2)  //choose which of the 4 byte will be loaded to basereg
                        2'b00: data_load_d = {24'b0, i_din[7:0]};
                        2'b01: data_load_d = {24'b0, i_din[15:8]};
                        2'b10: data_load_d = {24'b0, i_din[23:16]};
                        2'b11: data_load_d = {24'b0, i_din[31:24]};
                    endcase
                    data_load_d = {{{24{!i_funct3[2]}} & {24{data_load_d[7]}}} , data_load_d[7:0]}; //signed and unsigned extension in 1 equation
                    wr_mask_d = 4'b0001<<addr_2; //mask 1 of the 4 bytes
                    data_store_d = i_rs2<<{addr_2,3'b000}; //i_rs2<<(addr_2*8) , align data to mask
                   end
            2'b01: begin //halfword load/store
                    data_load_d = addr_2[1]? {16'b0,i_din[31:16]}: {16'b0,i_din[15:0]}; //choose which of the 2 halfwords will be loaded to basereg
                    data_load_d = {{{16{!i_funct3[2]}} & {16{data_load_d[15]}}},data_load_d[15:0]}; //signed and unsigned extension in 1 equation
                    wr_mask_d = 4'b0011<<{addr_2[1],1'b0}; //mask either the upper or lower half-word
                    data_store_d = i_rs2<<{addr_2[1],4'b0000}; //i_rs2<<(addr_2[1]*16) , align data to mask
                   end
            2'b10: begin //word load/store
                    data_load_d = i_din;
                    wr_mask_d = 4'b1111; //mask all
                    data_store_d = i_rs2;
                   end
          default: begin
                    data_store_d = 0;
                    data_load_d = 0;
                    wr_mask_d = 0; 
                   end
        endcase
    end


endmodule
