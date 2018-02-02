#include "Endianness.h"

#include "preprocessor/llvm_includes_start.h"
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/Support/Host.h>
#include "preprocessor/llvm_includes_end.h"

#include "Type.h"

namespace dev
{
namespace eth
{
namespace jit
{

llvm::Value* Endianness::bswapIfLE(IRBuilder& _builder, llvm::Value* _value)
{
	if (llvm::sys::IsLittleEndianHost)
	{
		if (auto constant = llvm::dyn_cast<llvm::ConstantInt>(_value))
			return _builder.getInt(constant->getValue().byteSwap());

		// OPT: Cache func declaration?
		auto bswapFunc = llvm::Intrinsic::getDeclaration(_builder.GetInsertBlock()->getParent()->getParent(), llvm::Intrinsic::bswap, _value->getType());
		return _builder.CreateCall(bswapFunc, _value);
	}
	return _value;
}

}
}
}
