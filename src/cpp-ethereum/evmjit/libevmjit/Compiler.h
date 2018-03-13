#pragma once

#include "JIT.h"
#include "BasicBlock.h"

namespace dev
{
namespace eth
{
namespace jit
{

class Compiler
{
public:

	struct Options
	{
		/// Rewrite switch instructions to sequences of branches
		bool rewriteSwitchToBranches = true;

		/// Dump CFG as a .dot file for graphviz
		bool dumpCFG = false;
	};

	Compiler(Options const& _options, evm_mode _mode, llvm::LLVMContext& _llvmContext);

	std::unique_ptr<llvm::Module> compile(code_iterator _begin, code_iterator _end, std::string const& _id);

private:

	std::vector<BasicBlock> createBasicBlocks(code_iterator _begin, code_iterator _end);

	void compileBasicBlock(BasicBlock& _basicBlock, class RuntimeManager& _runtimeManager, class Arith256& _arith, class Memory& _memory, class Ext& _ext, class GasMeter& _gasMeter);

	void resolveJumps();

	/// Compiler options
	Options const& m_options;

	/// EVM compatibility mode.
	evm_mode m_mode;

	/// Helper class for generating IR
	IRBuilder m_builder;

	/// Block with a jump table.
	llvm::BasicBlock* m_jumpTableBB = nullptr;

	/// Main program function
	llvm::Function* m_mainFunc = nullptr;
};

}
}
}
