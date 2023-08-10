//////////////////////////////////////////////////////////////////////////////////
// Company: UQAC
// Engineer: SHACHA
// 
// Create Date: 08/07/2023 12:05:47 AM
// Design Name: 
// Module Name: as_rv32i_alu
// Project Name: 
// Target Devices: 
// Tool Versions: 
// Description: Pipeline Stage 3 executes instruction [EXECUTE STAGE]
/** 
 * @file
 * @brief Pipeline Stage 3 ALU (Arithmetic Logic Unit) module documentation
 * 
 * This module acts as the Arithmetic Logic Unit (ALU) of the RISC-V core's third pipeline stage (EXECUTE STAGE).
 * It performs arithmetic, logic, and comparison operations based on instructions and operands from previous pipeline stages.
 * The ALU plays a vital role in processing instructions and computing results for program execution.
 * Key functionalities include:
 *   - Operand Selection: The ALU selects relevant operands for the ALU operation based on the opcode and instruction type.
 *   - ALU Operation: It performs various ALU operations such as ADD, SUB, SLT, SLTU, XOR, OR, AND, SLL, SRL, SRA, EQ, NEQ, GE, and GEU.
 *   - Branch and Jump Handling: The ALU computes the next PC value for branch and jump instructions and sets o_change_pc signal.
 *   - Register Writeback: Computes value for destination register (rd) and sets control signals for writeback.
 *   - Stalling and Flushing: Manages pipeline stalling and flushing based on input signals.
 * 
 * 
 * 
 * Dependencies: 
 * 
 * Revision:
 * Revision 0.01 - File Created
 * Additional Comments:
 * 
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

module as_rv32i_alu(
    input wire i_clk,i_rst_n,
    input wire[`ALU_WIDTH-1:0] i_alu,               ///< ALU operation type from previous stage
    input wire[4:0] i_rs1_addr,                     ///< Address for register source 1
    output reg[4:0] o_rs1_addr,                     ///< Address for register source 1
    input wire[31:0] i_rs1,                         ///< Source register 1 value
    output reg[31:0] o_rs1,                         ///< Source register 1 value
    input wire[31:0] i_rs2,                         ///< Source register 2 value
    output reg[31:0] o_rs2,                         ///< Source register 2 value
    input wire[31:0] i_imm,                         ///< Immediate value from previous stage
    output reg[11:0] o_imm,                         ///< Immediate value
    input wire[2:0] i_funct3,                       ///< Function type from previous stage
    output reg[2:0] o_funct3,                       ///< Function type
    input wire[`OPCODE_WIDTH-1:0] i_opcode,         ///< Opcode type from previous stage
    output reg[`OPCODE_WIDTH-1:0] o_opcode,         ///< Opcode type 
    input wire[`EXCEPTION_WIDTH-1:0] i_exception,   ///< Exception from decoder stage
    output reg[`EXCEPTION_WIDTH-1:0] o_exception,   ///< Exception: illegal inst,ecall,ebreak,mret
    output reg[31:0] o_y,                           ///< Result of arithmetic operation

    /// PC Control ///
    input wire[31:0] i_pc,      ///< Program Counter
    output reg[31:0] o_pc,      ///< pc register in pipeline
    output reg[31:0] o_next_pc, ///< New pc value
    output reg o_change_pc,     ///< High if pc needs to branch

    /// Basereg Control ///
    output reg o_wr_rd,         ///< Write rd to the base reg if enabled
    input wire[4:0] i_rd_addr,  ///< Address for destination register (from previous stage)
    output reg[4:0] o_rd_addr,  ///< Address for destination register
    output reg[31:0] o_rd,      ///< Value to be written back to destination register
    output reg o_rd_valid,      ///< High if o_rd is valid (not load nor csr instruction)

    /// Pipeline Control ///
    output reg o_stall_from_alu,    ///< Prepare to stall next stage(memory-access stage) for load/store instruction
    input wire i_ce,                ///< Input clk enable for pipeline stalling of this stage
    output reg o_ce,                ///< Output clk enable for pipeline stalling of next stage
    input wire i_stall,             ///< Informs this stage to stall
    input wire i_force_stall,       ///< Force this stage to stall
    output reg o_stall,             ///< Informs pipeline to stall
    input wire i_flush,             ///< Flush this stage
    output reg o_flush              ///< Flush previous stages
);

    wire alu_add = i_alu[`ADD];
    wire alu_sub = i_alu[`SUB];
    wire alu_slt = i_alu[`SLT];
    wire alu_sltu = i_alu[`SLTU];
    wire alu_xor = i_alu[`XOR];
    wire alu_or = i_alu[`OR];
    wire alu_and = i_alu[`AND];
    wire alu_sll = i_alu[`SLL];
    wire alu_srl = i_alu[`SRL];
    wire alu_sra = i_alu[`SRA];
    wire alu_eq = i_alu[`EQ];
    wire alu_neq = i_alu[`NEQ];
    wire alu_ge = i_alu[`GE];
    wire alu_geu = i_alu[`GEU];
    wire opcode_rtype = i_opcode[`RTYPE];
    wire opcode_itype = i_opcode[`ITYPE];
    wire opcode_load = i_opcode[`LOAD];
    wire opcode_store = i_opcode[`STORE];
    wire opcode_branch = i_opcode[`BRANCH];
    wire opcode_jal = i_opcode[`JAL];
    wire opcode_jalr = i_opcode[`JALR];
    wire opcode_lui = i_opcode[`LUI];
    wire opcode_auipc = i_opcode[`AUIPC];
    wire opcode_system = i_opcode[`SYSTEM];
    wire opcode_fence = i_opcode[`FENCE];

    /// ALU Internal Registers ///
    reg[31:0] a;    ///< Operand A
    reg[31:0] b;    ///< Operand B
    reg[31:0] y_d;  ///< ALU output
    reg[31:0] rd_d; ///< Next value to be written back to destination register
    reg wr_rd_d;    ///< Write rd to basereg if enabled
    reg rd_valid_d; ///< High if rd is valid (not load nor csr instruction)
    reg[31:0] a_pc;
    wire[31:0] sum;
    wire stall_bit = o_stall || i_stall;

    /// Register the output of i_alu ///
    always @(posedge i_clk, negedge i_rst_n) begin
        if(!i_rst_n) begin
            o_exception <= 0;
            o_ce <= 0;
            o_stall_from_alu <= 0;
        end
        else begin
            if(i_ce && !stall_bit) begin ///< Update register only if this stage is enabled
                o_opcode         <= i_opcode;
                o_exception      <= i_exception;
                o_y              <= y_d; 
                o_rs1_addr       <= i_rs1_addr;
                o_rs1            <= i_rs1;
                o_rs2            <= i_rs2;
                o_rd_addr        <= i_rd_addr;
                o_imm            <= i_imm[11:0];
                o_funct3         <= i_funct3;
                o_rd             <= rd_d;
                o_rd_valid       <= rd_valid_d;
                o_wr_rd          <= wr_rd_d;
                o_stall_from_alu <= i_opcode[`STORE] || i_opcode[`LOAD]; ///< Stall next stage(4th memory-access stage) when need to store/load since accessing data memory usually takes more than 1 cycle
                o_pc             <= i_pc;
            end
            if(i_flush && !stall_bit) begin ///< Flush this stage so clock-enable of next stage is disabled at next clock cycle
                o_ce <= 0;
            end
            else if(!stall_bit) begin ///< Clock-enable will change only when not stalled
                o_ce <= i_ce;
            end
            else if(stall_bit && !i_stall) o_ce <= 0; ///< If this stage is stalled but the next stage is not, disable the following stage's clock enablement at the next clock cycle (pipeline bubble).
        end
        
    end 

    /// Determine operation used then compute for y (ALU output) ///
    always @* begin  
        y_d = 0;
        
        a = (opcode_jal || opcode_auipc)? i_pc:i_rs1;  ///< a can either be pc or rs1
        b = (opcode_rtype || opcode_branch)? i_rs2:i_imm; ///< b can either be rs2 or imm 
        
        if(alu_add) y_d = a + b;
        if(alu_sub) y_d = a - b;
        if(alu_slt || alu_sltu) begin
            y_d = {31'b0, (a < b)};
            if(alu_slt) y_d = (a[31] ^ b[31])? {31'b0,a[31]}:y_d;
        end 
        if(alu_xor) y_d = a ^ b;
        if(alu_or)  y_d = a | b;
        if(alu_and) y_d = a & b;
        if(alu_sll) y_d = a << b[4:0];
        if(alu_srl) y_d = a >> b[4:0];
        if(alu_sra) y_d = $signed(a) >>> b[4:0];
        if(alu_eq || alu_neq) begin
            y_d = {31'b0, (a == b)};
            if(alu_neq) y_d = {31'b0,!y_d[0]};
        end
        if(alu_ge || alu_geu) begin
            y_d = {31'b0, (a >= b)};
            if(alu_ge) y_d = (a[31] ^ b[31])? {31'b0, b[31]}:y_d;
        end
    end

    assign sum = a_pc + i_imm; ///< Share adder for all addition operation for less resource utilization

    /// Determine o_rd to be saved to basereg and next value of PC ///
    always @* begin
        o_flush = i_flush; ///< Flush this stage along with the previous stages
        rd_d = 0;
        rd_valid_d = 0;
        o_change_pc = 0;
        o_next_pc = 0;
        wr_rd_d = 0;
        a_pc = i_pc;
        if(!i_flush) begin
            if(opcode_rtype || opcode_itype) rd_d = y_d;
            if(opcode_branch && y_d[0]) begin
                    o_next_pc = sum;    ///< Branch if value of ALU is 1(true)
                    o_change_pc = i_ce; ///< Change PC when ce of this stage is high (o_change_pc is valid)
                    o_flush = i_ce;
            end
            if(opcode_jal || opcode_jalr) begin
                if(opcode_jalr) a_pc = i_rs1;
                o_next_pc = sum;    ///< Jump to new PC
                o_change_pc = i_ce; ///< Change PC when ce of this stage is high (make o_change_pc valid)
                o_flush = i_ce;
                rd_d = i_pc + 4;    ///< Register the next pc value to destination register
            end 
        end
        if(opcode_lui) rd_d = i_imm;
        if(opcode_auipc) rd_d = sum;

        if(opcode_branch || opcode_store || (opcode_system && i_funct3 == 0) || opcode_fence ) wr_rd_d = 0; ///< i_funct3 == 0 are the non-csr system instructions 
        else wr_rd_d = 1; ///< Always write to the destination reg except when instruction is BRANCH or STORE or SYSTEM(except CSR system instruction)  

        if(opcode_load || (opcode_system && i_funct3!=0)) rd_valid_d = 0;  ///< Value of o_rd for load and CSR write is not yet available at this stage
        else rd_valid_d = 1;

        ///< Stall logic (stall when upper stages are stalled, when forced to stall, or when needs to flush previous stages but are still stalled)
        o_stall = (i_stall || i_force_stall) && !i_flush; ///< Stall when alu needs wait time
    end
        
    
    /********************************** TO DO: Formal Verification | Yosys *********************************/
    `ifdef FORMAL
        /// Assumption on inputs (not more than one opcode and alu operation is high) ///
        wire[4:0] f_alu=i_alu[`ADD]+i_alu[`SUB]+i_alu[`SLT]+i_alu[`SLTU]+i_alu[`XOR]+i_alu[`OR]+i_alu[`AND]+i_alu[`SLL]+i_alu[`SRL]+i_alu[`SRA]+i_alu[`EQ]+i_alu[`NEQ]+i_alu[`GE]+i_alu[`GEU]+0;
        wire[4:0] f_opcode=i_opcode[`RTYPE]+i_opcode[`ITYPE]+i_opcode[`LOAD]+i_opcode[`STORE]+i_opcode[`BRANCH]+i_opcode[`JAL]+i_opcode[`JALR]+i_opcode[`LUI]+i_opcode[`AUIPC]+i_opcode[`SYSTEM]+i_opcode[`FENCE];

        always @* begin
            assume(f_alu <= 1);
            assume(f_opcode <= 1);
        end

        /// verify all operations with $signed/$unsigned distinctions ///
        always @* begin 
            if(i_alu[`SLTU]) assert(y_d[0] == $unsigned(a) < $unsigned(b));
            if(i_alu[`SLT])  assert(y_d[0] == $signed(a) < $signed(b));
            if(i_alu[`SLL])  assert($unsigned(y_d) == $unsigned(a) << $unsigned(b[4:0]));
            if(i_alu[`SRL])  assert($unsigned(y_d) == $unsigned(a) >> $unsigned(b[4:0]));
            if(i_alu[`SRA])  assert($signed(y_d) == ($signed(a) >>> $unsigned(b[4:0])));
            if(i_alu[`GEU])  assert(y_d[0] == ($unsigned(a) >= $unsigned(b)));
            if(i_alu[`GE])   assert(y_d[0] == ($signed(a) >= $signed(b)));
        end
    `endif
    /********************************************************************************************************/

endmodule
