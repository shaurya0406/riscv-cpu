module  rv32_immediate_adder (
	input [31:0] pc_in, rs_1_in, imm_in,
	input iadder_src_in,
	output iadder_out
);
	
	assign iadder_out = (iadder_src_in) ? (imm_in + rs_1_in) : (imm_in + pc_in);
	
endmodule