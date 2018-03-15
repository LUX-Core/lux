#pragma once

#include <cstdint>
#include <cstring>
#include <functional>
#include <type_traits>

#include <evmjit.h>

namespace dev
{
namespace evmjit
{

using byte = uint8_t;
using bytes_ref = std::tuple<byte const*, size_t>;

/// Representation of 256-bit value binary compatible with LLVM i256
struct i256
{
	uint64_t words[4];

	i256() = default;
};

// TODO: Merge with ExecutionContext
struct RuntimeData
{
	enum Index
	{
		Gas,
		GasPrice,
		CallData,
		CallDataSize,
		ApparentCallValue,		// value of msg.value - different during DELEGATECALL
		Code,
		CodeSize,

		ReturnData 		   = CallData,		///< Return data pointer (set only in case of RETURN)
		ReturnDataSize 	   = CallDataSize,	///< Return data size (set only in case of RETURN)
	};

	static size_t const numElements = CodeSize + 1;

	int64_t 	gas = 0;
	int64_t 	gasPrice = 0;
	byte const* callData = nullptr;
	uint64_t 	callDataSize = 0;
	i256 		apparentValue;
	byte const* code = nullptr;
	uint64_t 	codeSize = 0;
};

struct JITSchedule
{
	// TODO: Move to constexpr once all our target compilers support it.
	typedef std::integral_constant<uint64_t, 1024> stackLimit;
	typedef std::integral_constant<uint64_t, 0> stepGas0;
	typedef std::integral_constant<uint64_t, 2> stepGas1;
	typedef std::integral_constant<uint64_t, 3> stepGas2;
	typedef std::integral_constant<uint64_t, 5> stepGas3;
	typedef std::integral_constant<uint64_t, 8> stepGas4;
	typedef std::integral_constant<uint64_t, 10> stepGas5;
	typedef std::integral_constant<uint64_t, 20> stepGas6;
	typedef std::integral_constant<uint64_t, 0> stepGas7;
	typedef std::integral_constant<uint64_t, 10> expByteGas;
	typedef std::integral_constant<uint64_t, 30> sha3Gas;
	typedef std::integral_constant<uint64_t, 6> sha3WordGas;
	typedef std::integral_constant<uint64_t, 50> sloadGas;
	typedef std::integral_constant<uint64_t, 20000> sstoreSetGas;
	typedef std::integral_constant<uint64_t, 5000> sstoreResetGas;
	typedef std::integral_constant<uint64_t, 5000> sstoreClearGas;
	typedef std::integral_constant<uint64_t, 1> jumpdestGas;
	typedef std::integral_constant<uint64_t, 375> logGas;
	typedef std::integral_constant<uint64_t, 8> logDataGas;
	typedef std::integral_constant<uint64_t, 375> logTopicGas;
	typedef std::integral_constant<uint64_t, 32000> createGas;
	typedef std::integral_constant<uint64_t, 40> callGas;
	typedef std::integral_constant<uint64_t, 3> memoryGas;
	typedef std::integral_constant<uint64_t, 3> copyGas;
	typedef std::integral_constant<uint64_t, 9000> valueTransferGas;
	typedef std::integral_constant<uint64_t, 2300> callStipend;
	typedef std::integral_constant<uint64_t, 25000> callNewAccount;
};

enum class ReturnCode
{
	// Success codes
	Stop    = 0,
	Return  = 1,

	// Standard error codes
	OutOfGas           = -1,

	// Internal error codes
	LLVMError          = -101,

	UnexpectedException = -111,
};

class ExecutionContext
{
public:
	ExecutionContext() = default;
	ExecutionContext(RuntimeData& _data, evm_env* _env) { init(_data, _env); }
	ExecutionContext(ExecutionContext const&) = delete;
	ExecutionContext& operator=(ExecutionContext const&) = delete;
	~ExecutionContext() noexcept;

	void init(RuntimeData& _data, evm_env* _env) { m_data = &_data; m_env = _env; }

	byte const* code() const { return m_data->code; }
	uint64_t codeSize() const { return m_data->codeSize; }

	bytes_ref getReturnData() const;

public:
	RuntimeData* m_data = nullptr;	///< Pointer to data. Expected by compiled contract.
	evm_env* m_env = nullptr;		///< Pointer to environment proxy. Expected by compiled contract.
	byte* m_memData = nullptr;
	uint64_t m_memSize = 0;
	uint64_t m_memCap = 0;

public:
	/// Reference to returned data (RETURN opcode used)
	bytes_ref returnData;
};

}
}
