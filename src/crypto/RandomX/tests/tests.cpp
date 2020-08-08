#ifdef NDEBUG
#undef NDEBUG
#endif

#include <cassert>
#include <iomanip>
#include "utility.hpp"
#include "../bytecode_machine.hpp"
#include "../dataset.hpp"
#include "../blake2/endian.h"
#include "../blake2/blake2.h"
#include "../blake2_generator.hpp"
#include "../superscalar.hpp"
#include "../reciprocal.h"
#include "../intrin_portable.h"
#include "../jit_compiler.hpp"
#include "../aes_hash.hpp"

randomx_cache* cache;
randomx_vm* vm = nullptr;

template<size_t N>
void initCache(const char (&key)[N]) {
	assert(cache != nullptr);
	randomx_init_cache(cache, key, N - 1);
	if (vm != nullptr)
		randomx_vm_set_cache(vm, cache);
}

template<size_t K, size_t H>
void calcStringHash(const char(&key)[K], const char(&input)[H], void* output) {
	initCache(key);
	assert(vm != nullptr);
	randomx_calculate_hash(vm, input, H - 1, output);
}

template<size_t K, size_t H>
void calcHexHash(const char(&key)[K], const char(&hex)[H], void* output) {
	initCache(key);
	assert(vm != nullptr);
	char input[H / 2];
	hex2bin((char*)hex, H - 1, input);
	randomx_calculate_hash(vm, input, sizeof(input), output);
}

int testNo = 0;
int skipped = 0;

template<typename FUNC>
void runTest(const char* name, bool condition, FUNC f) {
	std::cout << "[";
	std::cout.width(2);
	std::cout << std::right << ++testNo << "] ";
	std::cout.width(40);
	std::cout << std::left << name << " ... ";
	std::cout.flush();
	if (condition) {
		f();
		std::cout << "PASSED" << std::endl;
	}
	else {
		std::cout << "SKIPPED" << std::endl;
		skipped++;
	}
}

int main() {
	char testHash[32];

	//std::cout << "Allocating randomx_cache..." << std::endl;
	cache = randomx_alloc_cache(RANDOMX_FLAG_DEFAULT);

	runTest("Cache initialization", RANDOMX_ARGON_ITERATIONS == 3 && RANDOMX_ARGON_LANES == 1 && RANDOMX_ARGON_MEMORY == 262144 && stringsEqual(RANDOMX_ARGON_SALT, "RandomX\x03"),	[]() {
		initCache("test key 000");
		uint64_t* cacheMemory = (uint64_t*)cache->memory;
		assert(cacheMemory[0] == 0x191e0e1d23c02186);
		assert(cacheMemory[1568413] == 0xf1b62fe6210bf8b1);
		assert(cacheMemory[33554431] == 0x1f47f056d05cd99b);
	});

	runTest("SuperscalarHash generator", RANDOMX_SUPERSCALAR_LATENCY == 170, []() {
		char sprogHash[32];
		randomx::SuperscalarProgram sprog;
		const char key[] = "test key 000";
		constexpr size_t keySize = sizeof(key) - 1;
		randomx::Blake2Generator gen(key, keySize);

		const char superscalarReferences[10][65] = {
			"d3a4a6623738756f77e6104469102f082eff2a3e60be7ad696285ef7dfc72a61",
			"f5e7e0bbc7e93c609003d6359208688070afb4a77165a552ff7be63b38dfbc86",
			"85ed8b11734de5b3e9836641413a8f36e99e89694f419c8cd25c3f3f16c40c5a",
			"5dd956292cf5d5704ad99e362d70098b2777b2a1730520be52f772ca48cd3bc0",
			"6f14018ca7d519e9b48d91af094c0f2d7e12e93af0228782671a8640092af9e5",
			"134be097c92e2c45a92f23208cacd89e4ce51f1009a0b900dbe83b38de11d791",
			"268f9392c20c6e31371a5131f82bd7713d3910075f2f0468baafaa1abd2f3187",
			"c668a05fd909714ed4a91e8d96d67b17e44329e88bc71e0672b529a3fc16be47",
			"99739351315840963011e4c5d8e90ad0bfed3facdcb713fe8f7138fbf01c4c94",
			"14ab53d61880471f66e80183968d97effd5492b406876060e595fcf9682f9295",
		};

		for (int i = 0; i < 10; ++i) {
			randomx::generateSuperscalar(sprog, gen);
			blake2b(sprogHash, sizeof(sprogHash), &sprog.programBuffer, sizeof(randomx::Instruction) * sprog.getSize(), nullptr, 0);
			assert(equalsHex(sprogHash, superscalarReferences[i]));
		}
	});

	runTest("randomx_reciprocal", true, []() {
		assert(randomx_reciprocal(3) == 12297829382473034410U);
		assert(randomx_reciprocal(13) == 11351842506898185609U);
		assert(randomx_reciprocal(33) == 17887751829051686415U);
		assert(randomx_reciprocal(65537) == 18446462603027742720U);
		assert(randomx_reciprocal(15000001) == 10316166306300415204U);
		assert(randomx_reciprocal(3845182035) == 10302264209224146340U);
		assert(randomx_reciprocal(0xffffffff) == 9223372039002259456U);
	});

	runTest("randomx_reciprocal_fast", RANDOMX_HAVE_FAST_RECIPROCAL, []() {
		assert(randomx_reciprocal_fast(3) == 12297829382473034410U);
		assert(randomx_reciprocal_fast(13) == 11351842506898185609U);
		assert(randomx_reciprocal_fast(33) == 17887751829051686415U);
		assert(randomx_reciprocal_fast(65537) == 18446462603027742720U);
		assert(randomx_reciprocal_fast(15000001) == 10316166306300415204U);
		assert(randomx_reciprocal_fast(3845182035) == 10302264209224146340U);
		assert(randomx_reciprocal_fast(0xffffffff) == 9223372039002259456U);
	});

	runTest("Dataset initialization (interpreter)", stringsEqual(RANDOMX_ARGON_SALT, "RandomX\x03"), []() {
		initCache("test key 000");
		uint64_t datasetItem[8];
		randomx::initDatasetItem(cache, (uint8_t*)&datasetItem, 0);
		assert(datasetItem[0] == 0x680588a85ae222db);
		randomx::initDatasetItem(cache, (uint8_t*)&datasetItem, 10000000);
		assert(datasetItem[0] == 0x7943a1f6186ffb72);
		randomx::initDatasetItem(cache, (uint8_t*)&datasetItem, 20000000);
		assert(datasetItem[0] == 0x9035244d718095e1);
		randomx::initDatasetItem(cache, (uint8_t*)&datasetItem, 30000000);
		assert(datasetItem[0] == 0x145a5091f7853099);
	});

	runTest("Dataset initialization (compiler)", RANDOMX_HAVE_COMPILER && stringsEqual(RANDOMX_ARGON_SALT, "RandomX\x03"), []() {
		initCache("test key 000");
		randomx::JitCompiler jit;
		jit.generateSuperscalarHash(cache->programs, cache->reciprocalCache);
		jit.generateDatasetInitCode();
#ifdef RANDOMX_FORCE_SECURE
		jit.enableExecution();
#else
		jit.enableAll();
#endif
		uint64_t datasetItem[8];
		jit.getDatasetInitFunc()(cache, (uint8_t*)&datasetItem, 0, 1);
		assert(datasetItem[0] == 0x680588a85ae222db);
		jit.getDatasetInitFunc()(cache, (uint8_t*)&datasetItem, 10000000, 10000001);
		assert(datasetItem[0] == 0x7943a1f6186ffb72);
		jit.getDatasetInitFunc()(cache, (uint8_t*)&datasetItem, 20000000, 20000001);
		assert(datasetItem[0] == 0x9035244d718095e1);
		jit.getDatasetInitFunc()(cache, (uint8_t*)&datasetItem, 30000000, 30000001);
		assert(datasetItem[0] == 0x145a5091f7853099);
	});

	runTest("AesGenerator1R", true, []() {
		char state[64] = { 0 };
		hex2bin("6c19536eb2de31b6c0065f7f116e86f960d8af0c57210a6584c3237b9d064dc7", 64, state);
		fillAes1Rx4<true>(state, sizeof(state), state);
		assert(equalsHex(state, "fa89397dd6ca422513aeadba3f124b5540324c4ad4b6db434394307a17c833ab"));
	});

	randomx::NativeRegisterFile reg;
	randomx::BytecodeMachine decoder;
	randomx::InstructionByteCode ibc;
	alignas(16) randomx::ProgramConfiguration config;
	constexpr int registerHigh = 192;
	constexpr int registerDst = 0;
	constexpr int registerSrc = 1;
	int pc = 0;
	constexpr uint32_t imm32 = 3234567890;
	constexpr uint64_t imm64 = signExtend2sCompl(imm32);

	decoder.beginCompilation(reg);

	runTest("IADD_RS (decode)", RANDOMX_FREQ_IADD_RS > 0, [&] {
		randomx::Instruction instr;
		instr.opcode = randomx::ceil_IADD_RS - 1;
		instr.dst = registerHigh | registerDst;
		instr.src = registerHigh | registerSrc;
		instr.mod = UINT8_MAX;
		instr.setImm32(imm32);
		decoder.compileInstruction(instr, pc, ibc);
		assert(ibc.type == randomx::InstructionType::IADD_RS);
		assert(ibc.idst == &reg.r[registerDst]);
		assert(ibc.isrc == &reg.r[registerSrc]);
		assert(ibc.shift == 3);
		assert(ibc.imm == 0);
	});

	runTest("IADD_RS (execute)", RANDOMX_FREQ_IADD_RS > 0, [&] {
		reg.r[registerDst] = 0x8000000000000000;
		reg.r[registerSrc] = 0x1000000000000000;
		decoder.executeInstruction(ibc, pc, nullptr, config);
		assert(reg.r[registerDst] == 0);
	});

	runTest("IADD_RS with immediate (decode)", RANDOMX_FREQ_IADD_RS > 0, [&] {
		randomx::Instruction instr;
		instr.opcode = randomx::ceil_IADD_RS - 1;
		instr.mod = 8;
		instr.dst = registerHigh | randomx::RegisterNeedsDisplacement;
		instr.src = registerHigh | registerSrc;
		instr.setImm32(imm32);
		decoder.compileInstruction(instr, pc, ibc);
		assert(ibc.type == randomx::InstructionType::IADD_RS);
		assert(ibc.idst == &reg.r[randomx::RegisterNeedsDisplacement]);
		assert(ibc.isrc == &reg.r[registerSrc]);
		assert(ibc.shift == 2);
		assert(ibc.imm == imm64);
	});

	runTest("IADD_RS with immediate (decode)", RANDOMX_FREQ_IADD_RS > 0, [&] {
		reg.r[randomx::RegisterNeedsDisplacement] = 0x8000000000000000;
		reg.r[registerSrc] = 0x2000000000000000;
		decoder.executeInstruction(ibc, pc, nullptr, config);
		assert(reg.r[randomx::RegisterNeedsDisplacement] == imm64);
	});

	runTest("IADD_M (decode)", RANDOMX_FREQ_IADD_M > 0, [&] {
		randomx::Instruction instr;
		instr.opcode = randomx::ceil_IADD_M - 1;
		instr.mod = 1;
		instr.dst = registerHigh | registerDst;
		instr.src = registerHigh | registerSrc;
		instr.setImm32(imm32);
		decoder.compileInstruction(instr, pc, ibc);
		assert(ibc.type == randomx::InstructionType::IADD_M);
		assert(ibc.idst == &reg.r[registerDst]);
		assert(ibc.isrc == &reg.r[registerSrc]);
		assert(ibc.imm == imm64);
		assert(ibc.memMask == randomx::ScratchpadL1Mask);
	});

	runTest("ISUB_R (decode)", RANDOMX_FREQ_ISUB_R > 0, [&] {
		randomx::Instruction instr;
		instr.opcode = randomx::ceil_ISUB_R - 1;
		instr.dst = registerHigh | registerDst;
		instr.src = registerHigh | registerSrc;
		instr.setImm32(imm32);
		decoder.compileInstruction(instr, pc, ibc);
		assert(ibc.type == randomx::InstructionType::ISUB_R);
		assert(ibc.idst == &reg.r[registerDst]);
		assert(ibc.isrc == &reg.r[registerSrc]);
	});

	runTest("ISUB_R (execute)", RANDOMX_FREQ_ISUB_R > 0, [&] {
		reg.r[registerDst] = 1;
		reg.r[registerSrc] = 0xFFFFFFFF;
		decoder.executeInstruction(ibc, pc, nullptr, config);
		assert(reg.r[registerDst] == 0xFFFFFFFF00000002);
	});

	runTest("ISUB_R with immediate (decode)", RANDOMX_FREQ_ISUB_R > 0, [&] {
		randomx::Instruction instr;
		instr.opcode = randomx::ceil_ISUB_R - 1;
		instr.dst = registerHigh | registerDst;
		instr.src = registerHigh | registerDst;
		instr.setImm32(imm32);
		decoder.compileInstruction(instr, pc, ibc);
		assert(ibc.type == randomx::InstructionType::ISUB_R);
		assert(ibc.idst == &reg.r[registerDst]);
		assert(ibc.isrc == &ibc.imm);
	});

	runTest("ISUB_R with immediate (decode)", RANDOMX_FREQ_ISUB_R > 0, [&] {
		reg.r[registerDst] = 0;
		decoder.executeInstruction(ibc, pc, nullptr, config);
		assert(reg.r[registerDst] == (~imm64 + 1));
	});

	runTest("ISUB_M (decode)", RANDOMX_FREQ_ISUB_M > 0, [&] {
		randomx::Instruction instr;
		instr.opcode = randomx::ceil_ISUB_M - 1;
		instr.mod = 0;
		instr.dst = registerHigh | registerDst;
		instr.src = registerHigh | registerSrc;
		instr.setImm32(imm32);
		decoder.compileInstruction(instr, pc, ibc);
		assert(ibc.type == randomx::InstructionType::ISUB_M);
		assert(ibc.idst == &reg.r[registerDst]);
		assert(ibc.isrc == &reg.r[registerSrc]);
		assert(ibc.imm == imm64);
		assert(ibc.memMask == randomx::ScratchpadL2Mask);
	});

	runTest("IMUL_R (decode)", RANDOMX_FREQ_IMUL_R > 0, [&] {
		randomx::Instruction instr;
		instr.opcode = randomx::ceil_IMUL_R - 1;
		instr.dst = registerHigh | registerDst;
		instr.src = registerHigh | registerSrc;
		instr.setImm32(imm32);
		decoder.compileInstruction(instr, pc, ibc);
		assert(ibc.type == randomx::InstructionType::IMUL_R);
		assert(ibc.idst == &reg.r[registerDst]);
		assert(ibc.isrc == &reg.r[registerSrc]);
	});

	runTest("IMUL_R (execute)", RANDOMX_FREQ_IMUL_R > 0, [&] {
		reg.r[registerDst] = 0xBC550E96BA88A72B;
		reg.r[registerSrc] = 0xF5391FA9F18D6273;
		decoder.executeInstruction(ibc, pc, nullptr, config);
		assert(reg.r[registerDst] == 0x28723424A9108E51);
	});

	runTest("IMUL_R with immediate (decode)", RANDOMX_FREQ_IMUL_R > 0, [&] {
		randomx::Instruction instr;
		instr.opcode = randomx::ceil_IMUL_R - 1;
		instr.dst = registerHigh | registerDst;
		instr.src = registerHigh | registerDst;
		instr.setImm32(imm32);
		decoder.compileInstruction(instr, pc, ibc);
		assert(ibc.type == randomx::InstructionType::IMUL_R);
		assert(ibc.idst == &reg.r[registerDst]);
		assert(ibc.isrc == &ibc.imm);
	});

	runTest("IMUL_R with immediate (execute)", RANDOMX_FREQ_IMUL_R > 0, [&] {
		reg.r[registerDst] = 1;
		decoder.executeInstruction(ibc, pc, nullptr, config);
		assert(reg.r[registerDst] == imm64);
	});

	runTest("IMUL_M (decode)", RANDOMX_FREQ_IMUL_M > 0, [&] {
		randomx::Instruction instr;
		instr.opcode = randomx::ceil_IMUL_M - 1;
		instr.mod = 0;
		instr.dst = registerHigh | registerDst;
		instr.src = registerHigh | registerDst;
		instr.setImm32(imm32);
		decoder.compileInstruction(instr, pc, ibc);
		assert(ibc.type == randomx::InstructionType::IMUL_M);
		assert(ibc.idst == &reg.r[registerDst]);
		assert(*ibc.isrc == 0);
		assert(ibc.imm == imm64);
		assert(ibc.memMask == randomx::ScratchpadL3Mask);
	});

	runTest("IMULH_R (decode)", RANDOMX_FREQ_IMULH_R > 0, [&] {
		randomx::Instruction instr;
		instr.opcode = randomx::ceil_IMULH_R - 1;
		instr.dst = registerHigh | registerDst;
		instr.src = registerHigh | registerSrc;
		instr.setImm32(imm32);
		decoder.compileInstruction(instr, pc, ibc);
		assert(ibc.type == randomx::InstructionType::IMULH_R);
		assert(ibc.idst == &reg.r[registerDst]);
		assert(ibc.isrc == &reg.r[registerSrc]);
	});

	runTest("IMULH_R (execute)", RANDOMX_FREQ_IMULH_R > 0, [&] {
		reg.r[registerDst] = 0xBC550E96BA88A72B;
		reg.r[registerSrc] = 0xF5391FA9F18D6273;
		decoder.executeInstruction(ibc, pc, nullptr, config);
		assert(reg.r[registerDst] == 0xB4676D31D2B34883);
	});

	runTest("IMULH_R squared (decode)", RANDOMX_FREQ_IMULH_R > 0, [&] {
		randomx::Instruction instr;
		instr.opcode = randomx::ceil_IMULH_R - 1;
		instr.dst = registerHigh | registerDst;
		instr.src = registerHigh | registerDst;
		instr.setImm32(imm32);
		decoder.compileInstruction(instr, pc, ibc);
		assert(ibc.type == randomx::InstructionType::IMULH_R);
		assert(ibc.idst == &reg.r[registerDst]);
		assert(ibc.isrc == &reg.r[registerDst]);
	});

	runTest("IMULH_M (decode)", RANDOMX_FREQ_IMULH_M > 0, [&] {
		randomx::Instruction instr;
		instr.opcode = randomx::ceil_IMULH_M - 1;
		instr.mod = 0;
		instr.dst = registerHigh | registerDst;
		instr.src = registerHigh | registerSrc;
		instr.setImm32(imm32);
		decoder.compileInstruction(instr, pc, ibc);
		assert(ibc.type == randomx::InstructionType::IMULH_M);
		assert(ibc.idst == &reg.r[registerDst]);
		assert(ibc.isrc == &reg.r[registerSrc]);
		assert(ibc.imm == imm64);
		assert(ibc.memMask == randomx::ScratchpadL2Mask);
	});

	runTest("ISMULH_R (decode)", RANDOMX_FREQ_ISMULH_R > 0, [&] {
		randomx::Instruction instr;
		instr.opcode = randomx::ceil_ISMULH_R - 1;
		instr.dst = registerHigh | registerDst;
		instr.src = registerHigh | registerSrc;
		instr.setImm32(imm32);
		decoder.compileInstruction(instr, pc, ibc);
		assert(ibc.type == randomx::InstructionType::ISMULH_R);
		assert(ibc.idst == &reg.r[registerDst]);
		assert(ibc.isrc == &reg.r[registerSrc]);
	});

	runTest("ISMULH_R (execute)", RANDOMX_FREQ_ISMULH_R > 0, [&] {
		reg.r[registerDst] = 0xBC550E96BA88A72B;
		reg.r[registerSrc] = 0xF5391FA9F18D6273;
		decoder.executeInstruction(ibc, pc, nullptr, config);
		assert(reg.r[registerDst] == 0x02D93EF1269D3EE5);
	});

	runTest("ISMULH_R squared (decode)", RANDOMX_FREQ_ISMULH_R > 0, [&] {
		randomx::Instruction instr;
		instr.opcode = randomx::ceil_ISMULH_R - 1;
		instr.dst = registerHigh | registerDst;
		instr.src = registerHigh | registerDst;
		instr.setImm32(imm32);
		decoder.compileInstruction(instr, pc, ibc);
		assert(ibc.type == randomx::InstructionType::ISMULH_R);
		assert(ibc.idst == &reg.r[registerDst]);
		assert(ibc.isrc == &reg.r[registerDst]);
	});

	runTest("ISMULH_M (decode)", RANDOMX_FREQ_ISMULH_M > 0, [&] {
		randomx::Instruction instr;
		instr.opcode = randomx::ceil_ISMULH_M - 1;
		instr.mod = 3;
		instr.dst = registerHigh | registerDst;
		instr.src = registerHigh | registerSrc;
		instr.setImm32(imm32);
		decoder.compileInstruction(instr, pc, ibc);
		assert(ibc.type == randomx::InstructionType::ISMULH_M);
		assert(ibc.idst == &reg.r[registerDst]);
		assert(ibc.isrc == &reg.r[registerSrc]);
		assert(ibc.imm == imm64);
		assert(ibc.memMask == randomx::ScratchpadL1Mask);
	});

	runTest("IMUL_RCP (decode)", RANDOMX_FREQ_IMUL_RCP > 0, [&] {
		randomx::Instruction instr;
		instr.opcode = randomx::ceil_IMUL_RCP - 1;
		instr.dst = registerHigh | registerDst;
		instr.setImm32(imm32);
		decoder.compileInstruction(instr, pc, ibc);
		assert(ibc.type == randomx::InstructionType::IMUL_R);
		assert(ibc.idst == &reg.r[registerDst]);
		assert(ibc.isrc == &ibc.imm);
		assert(ibc.imm == randomx_reciprocal(imm32));
	});

	runTest("IMUL_RCP zero imm32 (decode)", RANDOMX_FREQ_IMUL_RCP > 0, [&] {
		randomx::Instruction instr;
		instr.opcode = randomx::ceil_IMUL_RCP - 1;
		instr.setImm32(0);
		decoder.compileInstruction(instr, pc, ibc);
		assert(ibc.type == randomx::InstructionType::NOP);
	});

	runTest("INEG_R (decode)", RANDOMX_FREQ_INEG_R > 0, [&] {
		randomx::Instruction instr;
		instr.opcode = randomx::ceil_INEG_R - 1;
		instr.dst = registerHigh | registerDst;
		instr.setImm32(imm32);
		decoder.compileInstruction(instr, pc, ibc);
		assert(ibc.type == randomx::InstructionType::INEG_R);
		assert(ibc.idst == &reg.r[registerDst]);
	});

	runTest("INEG_R (execute)", RANDOMX_FREQ_INEG_R > 0, [&] {
		reg.r[registerDst] = 0xFFFFFFFFFFFFFFFF;
		decoder.executeInstruction(ibc, pc, nullptr, config);
		assert(reg.r[registerDst] == 1);
	});
	
	runTest("IXOR_R (decode)", RANDOMX_FREQ_IXOR_R > 0, [&] {
		randomx::Instruction instr;
		instr.opcode = randomx::ceil_IXOR_R - 1;
		instr.dst = registerHigh | registerDst;
		instr.src = registerHigh | registerSrc;
		instr.setImm32(imm32);
		decoder.compileInstruction(instr, pc, ibc);
		assert(ibc.type == randomx::InstructionType::IXOR_R);
		assert(ibc.idst == &reg.r[registerDst]);
		assert(ibc.isrc == &reg.r[registerSrc]);
	});

	runTest("IXOR_R (execute)", RANDOMX_FREQ_IMUL_R > 0, [&] {
		reg.r[registerDst] = 0x8888888888888888;
		reg.r[registerSrc] = 0xAAAAAAAAAAAAAAAA;
		decoder.executeInstruction(ibc, pc, nullptr, config);
		assert(reg.r[registerDst] == 0x2222222222222222);
	});

	runTest("IXOR_R with immediate (decode)", RANDOMX_FREQ_IXOR_R > 0, [&] {
		randomx::Instruction instr;
		instr.opcode = randomx::ceil_IXOR_R - 1;
		instr.dst = registerHigh | registerDst;
		instr.src = registerHigh | registerDst;
		instr.setImm32(imm32);
		decoder.compileInstruction(instr, pc, ibc);
		assert(ibc.type == randomx::InstructionType::IXOR_R);
		assert(ibc.idst == &reg.r[registerDst]);
		assert(ibc.isrc == &ibc.imm);
	});

	runTest("IXOR_R with immediate (execute)", RANDOMX_FREQ_IXOR_R > 0, [&] {
		reg.r[registerDst] = 0xFFFFFFFFFFFFFFFF;
		decoder.executeInstruction(ibc, pc, nullptr, config);
		assert(reg.r[registerDst] == ~imm64);
	});

	runTest("IXOR_M (decode)", RANDOMX_FREQ_IXOR_M > 0, [&] {
		randomx::Instruction instr;
		instr.opcode = randomx::ceil_IXOR_M - 1;
		instr.dst = registerHigh | registerDst;
		instr.src = registerHigh | registerDst;
		instr.setImm32(imm32);
		decoder.compileInstruction(instr, pc, ibc);
		assert(ibc.type == randomx::InstructionType::IXOR_M);
		assert(ibc.idst == &reg.r[registerDst]);
		assert(*ibc.isrc == 0);
		assert(ibc.imm == imm64);
		assert(ibc.memMask == randomx::ScratchpadL3Mask);
	});

	runTest("IROR_R (decode)", RANDOMX_FREQ_IROR_R > 0, [&] {
		randomx::Instruction instr;
		instr.opcode = randomx::ceil_IROR_R - 1;
		instr.dst = registerHigh | registerDst;
		instr.src = registerHigh | registerSrc;
		instr.setImm32(imm32);
		decoder.compileInstruction(instr, pc, ibc);
		assert(ibc.type == randomx::InstructionType::IROR_R);
		assert(ibc.idst == &reg.r[registerDst]);
		assert(ibc.isrc == &reg.r[registerSrc]);
	});

	runTest("IROR_R (execute)", RANDOMX_FREQ_IROR_R > 0, [&] {
		reg.r[registerDst] = 953360005391419562;
		reg.r[registerSrc] = 4569451684712230561;
		decoder.executeInstruction(ibc, pc, nullptr, config);
		assert(reg.r[registerDst] == 0xD835C455069D81EF);
	});

	runTest("IROL_R (decode)", RANDOMX_FREQ_IROL_R > 0, [&] {
		randomx::Instruction instr;
		instr.opcode = randomx::ceil_IROL_R - 1;
		instr.dst = registerHigh | registerDst;
		instr.src = registerHigh | registerSrc;
		instr.setImm32(imm32);
		decoder.compileInstruction(instr, pc, ibc);
		assert(ibc.type == randomx::InstructionType::IROL_R);
		assert(ibc.idst == &reg.r[registerDst]);
		assert(ibc.isrc == &reg.r[registerSrc]);
	});

	runTest("IROL_R (execute)", RANDOMX_FREQ_IROL_R > 0, [&] {
		reg.r[registerDst] = 953360005391419562;
		reg.r[registerSrc] = 4569451684712230561;
		decoder.executeInstruction(ibc, pc, nullptr, config);
		assert(reg.r[registerDst] == 6978065200552740799);
	});

	runTest("ISWAP_R (decode)", RANDOMX_FREQ_ISWAP_R > 0, [&] {
		randomx::Instruction instr;
		instr.opcode = randomx::ceil_ISWAP_R - 1;
		instr.dst = registerHigh | registerDst;
		instr.src = registerHigh | registerSrc;
		instr.setImm32(imm32);
		decoder.compileInstruction(instr, pc, ibc);
		assert(ibc.type == randomx::InstructionType::ISWAP_R);
		assert(ibc.idst == &reg.r[registerDst]);
		assert(ibc.isrc == &reg.r[registerSrc]);
	});

	runTest("ISWAP_R (execute)", RANDOMX_FREQ_ISWAP_R > 0, [&] {
		reg.r[registerDst] = 953360005391419562;
		reg.r[registerSrc] = 4569451684712230561;
		decoder.executeInstruction(ibc, pc, nullptr, config);
		assert(reg.r[registerDst] == 4569451684712230561);
		assert(reg.r[registerSrc] == 953360005391419562);
	});

	runTest("FSWAP_R (decode)", RANDOMX_FREQ_FSWAP_R > 0, [&] {
		randomx::Instruction instr;
		instr.opcode = randomx::ceil_FSWAP_R - 1;
		instr.dst = registerHigh | registerDst;
		decoder.compileInstruction(instr, pc, ibc);
		assert(ibc.type == randomx::InstructionType::FSWAP_R);
		assert(ibc.fdst == &reg.f[registerDst]);
	});

	runTest("FSWAP_R (execute)", RANDOMX_FREQ_FSWAP_R > 0, [&] {
		alignas(16) uint64_t vec[2];
		reg.f[registerDst] = rx_set_vec_f128(953360005391419562, 4569451684712230561);
		decoder.executeInstruction(ibc, pc, nullptr, config);
		rx_store_vec_f128((double*)&vec, reg.f[registerDst]);
		assert(equalsHex((const char*)&vec, "aa886bb0df033b0da12e95e518f4693f"));
	});

	runTest("FADD_R (decode)", RANDOMX_FREQ_FADD_R > 0, [&] {
		randomx::Instruction instr;
		instr.opcode = randomx::ceil_FADD_R - 1;
		instr.dst = registerHigh | registerDst;
		instr.src = registerHigh | registerSrc;
		instr.setImm32(imm32);
		decoder.compileInstruction(instr, pc, ibc);
		assert(ibc.type == randomx::InstructionType::FADD_R);
		assert(ibc.fdst == &reg.f[registerDst]);
		assert(ibc.fsrc == &reg.a[registerSrc]);
	});

	runTest("FADD_R RoundToNearest (execute)", RANDOMX_FREQ_FADD_R > 0, [&] {
		alignas(16) uint64_t vec[2];
		reg.f[registerDst] = rx_set_vec_f128(0x3ffd2c97cc4ef015, 0xc1ce30b3c4223576);
		reg.a[registerSrc] = rx_set_vec_f128(0x402a26a86a60c8fb, 0x40b8f684057a59e1);
		rx_set_rounding_mode(RoundToNearest);
		decoder.executeInstruction(ibc, pc, nullptr, config);
		rx_store_vec_f128((double*)&vec, reg.f[registerDst]);
		assert(equalsHex(&vec, "b932e048a730cec1fea6ea633bcc2d40"));
	});

	runTest("FADD_R RoundDown (execute)", RANDOMX_FREQ_FADD_R > 0, [&] {
		alignas(16) uint64_t vec[2];
		reg.f[registerDst] = rx_set_vec_f128(0x3ffd2c97cc4ef015, 0xc1ce30b3c4223576);
		reg.a[registerSrc] = rx_set_vec_f128(0x402a26a86a60c8fb, 0x40b8f684057a59e1);
		rx_set_rounding_mode(RoundDown);
		decoder.executeInstruction(ibc, pc, nullptr, config);
		rx_store_vec_f128((double*)&vec, reg.f[registerDst]);
		assert(equalsHex(&vec, "b932e048a730cec1fda6ea633bcc2d40"));
	});

	runTest("FADD_R RoundUp (execute)", RANDOMX_FREQ_FADD_R > 0, [&] {
		alignas(16) uint64_t vec[2];
		reg.f[registerDst] = rx_set_vec_f128(0x3ffd2c97cc4ef015, 0xc1ce30b3c4223576);
		reg.a[registerSrc] = rx_set_vec_f128(0x402a26a86a60c8fb, 0x40b8f684057a59e1);
		rx_set_rounding_mode(RoundUp);
		decoder.executeInstruction(ibc, pc, nullptr, config);
		rx_store_vec_f128((double*)&vec, reg.f[registerDst]);
		assert(equalsHex(&vec, "b832e048a730cec1fea6ea633bcc2d40"));
	});

	runTest("FADD_R RoundToZero (execute)", RANDOMX_FREQ_FADD_R > 0, [&] {
		alignas(16) uint64_t vec[2];
		reg.f[registerDst] = rx_set_vec_f128(0x3ffd2c97cc4ef015, 0xc1ce30b3c4223576);
		reg.a[registerSrc] = rx_set_vec_f128(0x402a26a86a60c8fb, 0x40b8f684057a59e1);
		rx_set_rounding_mode(RoundToZero);
		decoder.executeInstruction(ibc, pc, nullptr, config);
		rx_store_vec_f128((double*)&vec, reg.f[registerDst]);
		assert(equalsHex(&vec, "b832e048a730cec1fda6ea633bcc2d40"));
	});

	runTest("FADD_M (decode)", RANDOMX_FREQ_FADD_M > 0, [&] {
		randomx::Instruction instr;
		instr.opcode = randomx::ceil_FADD_M - 1;
		instr.mod = 1;
		instr.dst = registerHigh | registerDst;
		instr.src = registerHigh | registerSrc;
		instr.setImm32(imm32);
		decoder.compileInstruction(instr, pc, ibc);
		assert(ibc.type == randomx::InstructionType::FADD_M);
		assert(ibc.fdst == &reg.f[registerDst]);
		assert(ibc.isrc == &reg.r[registerSrc]);
		assert(ibc.imm == imm64);
		assert(ibc.memMask == randomx::ScratchpadL1Mask);
	});

	runTest("FADD_M (execute)", RANDOMX_FREQ_FADD_R > 0, [&] {
		uint64_t mockScratchpad;
		store64(&mockScratchpad, 0x1234567890abcdef);
		alignas(16) uint64_t vec[2];
		reg.f[registerDst] = rx_set_vec_f128(0, 0);
		reg.r[registerSrc] = 0xFFFFFFFFFFFFE930;
		rx_set_rounding_mode(RoundToNearest);
		decoder.executeInstruction(ibc, pc, (uint8_t*)&mockScratchpad, config);
		rx_store_vec_f128((double*)&vec, reg.f[registerDst]);
		assert(equalsHex(&vec, "000040840cd5dbc1000000785634b241"));
	});

	runTest("FSUB_R (decode)", RANDOMX_FREQ_FSUB_R > 0, [&] {
		randomx::Instruction instr;
		instr.opcode = randomx::ceil_FSUB_R - 1;
		instr.dst = registerHigh | registerDst;
		instr.src = registerHigh | registerSrc;
		instr.setImm32(imm32);
		decoder.compileInstruction(instr, pc, ibc);
		assert(ibc.type == randomx::InstructionType::FSUB_R);
		assert(ibc.fdst == &reg.f[registerDst]);
		assert(ibc.fsrc == &reg.a[registerSrc]);
	});

	runTest("FSUB_M (decode)", RANDOMX_FREQ_FSUB_M > 0, [&] {
		randomx::Instruction instr;
		instr.opcode = randomx::ceil_FSUB_M - 1;
		instr.mod = 2;
		instr.dst = registerHigh | registerDst;
		instr.src = registerHigh | registerSrc;
		instr.setImm32(imm32);
		decoder.compileInstruction(instr, pc, ibc);
		assert(ibc.type == randomx::InstructionType::FSUB_M);
		assert(ibc.fdst == &reg.f[registerDst]);
		assert(ibc.isrc == &reg.r[registerSrc]);
		assert(ibc.imm == imm64);
		assert(ibc.memMask == randomx::ScratchpadL1Mask);
	});

	runTest("FSCAL_R (decode)", RANDOMX_FREQ_FSCAL_R > 0, [&] {
		randomx::Instruction instr;
		instr.opcode = randomx::ceil_FSCAL_R - 1;
		instr.dst = registerHigh | registerDst;
		instr.setImm32(imm32);
		decoder.compileInstruction(instr, pc, ibc);
		assert(ibc.type == randomx::InstructionType::FSCAL_R);
		assert(ibc.fdst == &reg.f[registerDst]);
	});

	runTest("FSCAL_R (execute)", RANDOMX_FREQ_FSCAL_R > 0, [&] {
		alignas(16) uint64_t vec[2];
		reg.f[registerDst] = rx_set_vec_f128(0x41dbc35cef248783, 0x40fdfdabb6173d07);
		decoder.executeInstruction(ibc, pc, nullptr, config);
		rx_store_vec_f128((double*)&vec, reg.f[registerDst]);
		assert(equalsHex((const char*)&vec, "073d17b6abfd0dc0838724ef5cc32bc1"));
	});

	runTest("FMUL_R (decode)", RANDOMX_FREQ_FMUL_R > 0, [&] {
		randomx::Instruction instr;
		instr.opcode = randomx::ceil_FMUL_R - 1;
		instr.dst = registerHigh | registerDst;
		instr.src = registerHigh | registerSrc;
		instr.setImm32(imm32);
		decoder.compileInstruction(instr, pc, ibc);
		assert(ibc.type == randomx::InstructionType::FMUL_R);
		assert(ibc.fdst == &reg.e[registerDst]);
		assert(ibc.fsrc == &reg.a[registerSrc]);
	});

	runTest("FMUL_R RoundToNearest (execute)", RANDOMX_FREQ_FMUL_R > 0, [&] {
		alignas(16) uint64_t vec[2];
		reg.e[registerDst] = rx_set_vec_f128(0x41dbc35cef248783, 0x40fdfdabb6173d07);
		reg.a[registerSrc] = rx_set_vec_f128(0x40eba861aa31c7c0, 0x41c4561212ae2d50);
		rx_set_rounding_mode(RoundToNearest);
		decoder.executeInstruction(ibc, pc, nullptr, config);
		rx_store_vec_f128((double*)&vec, reg.e[registerDst]);
		assert(equalsHex(&vec, "69697aff350fd3422f1589cdecfed742"));
	});

	runTest("FMUL_R RoundDown/RoundToZero (execute)", RANDOMX_FREQ_FMUL_R > 0, [&] {
		alignas(16) uint64_t vec[2];
		reg.e[registerDst] = rx_set_vec_f128(0x41dbc35cef248783, 0x40fdfdabb6173d07);
		reg.a[registerSrc] = rx_set_vec_f128(0x40eba861aa31c7c0, 0x41c4561212ae2d50);
		rx_set_rounding_mode(RoundDown);
		decoder.executeInstruction(ibc, pc, nullptr, config);
		rx_store_vec_f128((double*)&vec, reg.e[registerDst]);
		assert(equalsHex(&vec, "69697aff350fd3422e1589cdecfed742"));
	});

	runTest("FMUL_R RoundUp (execute)", RANDOMX_FREQ_FMUL_R > 0, [&] {
		alignas(16) uint64_t vec[2];
		reg.e[registerDst] = rx_set_vec_f128(0x41dbc35cef248783, 0x40fdfdabb6173d07);
		reg.a[registerSrc] = rx_set_vec_f128(0x40eba861aa31c7c0, 0x41c4561212ae2d50);
		rx_set_rounding_mode(RoundUp);
		decoder.executeInstruction(ibc, pc, nullptr, config);
		rx_store_vec_f128((double*)&vec, reg.e[registerDst]);
		assert(equalsHex(&vec, "6a697aff350fd3422f1589cdecfed742"));
	});

	runTest("FDIV_M (decode)", RANDOMX_FREQ_FDIV_M > 0, [&] {
		randomx::Instruction instr;
		instr.opcode = randomx::ceil_FDIV_M - 1;
		instr.mod = 3;
		instr.dst = registerHigh | registerDst;
		instr.src = registerHigh | registerSrc;
		instr.setImm32(imm32);
		decoder.compileInstruction(instr, pc, ibc);
		assert(ibc.type == randomx::InstructionType::FDIV_M);
		assert(ibc.fdst == &reg.e[registerDst]);
		assert(ibc.isrc == &reg.r[registerSrc]);
		assert(ibc.imm == imm64);
		assert(ibc.memMask == randomx::ScratchpadL1Mask);
	});

	runTest("FDIV_M RoundToNearest (execute)", RANDOMX_FREQ_FDIV_M > 0, [&] {
		alignas(16) uint64_t vec[2];
		alignas(16) uint32_t mockScratchpad[2];
		store32(&mockScratchpad[0], 0xd350a1b6);
		store32(&mockScratchpad[1], 0x8b2460d9);
		store64(&config.eMask[0], 0x3a0000000005d11a);
		store64(&config.eMask[1], 0x39000000001ba31e);
		reg.e[registerDst] = rx_set_vec_f128(0x41937f76fede16ee, 0x411b414296ce93b6);
		reg.r[registerSrc] = 0xFFFFFFFFFFFFE930;
		rx_set_rounding_mode(RoundToNearest);
		decoder.executeInstruction(ibc, pc, (uint8_t*)&mockScratchpad, config);
		rx_store_vec_f128((double*)&vec, reg.e[registerDst]);
		assert(equalsHex(&vec, "e7b269639484434632474a66635ba547"));
	});

	runTest("FDIV_M RoundDown/RoundToZero (execute)", RANDOMX_FREQ_FDIV_M > 0, [&] {
		alignas(16) uint64_t vec[2];
		alignas(16) uint32_t mockScratchpad[2];
		store32(&mockScratchpad[0], 0xd350a1b6);
		store32(&mockScratchpad[1], 0x8b2460d9);
		store64(&config.eMask[0], 0x3a0000000005d11a);
		store64(&config.eMask[1], 0x39000000001ba31e);
		reg.e[registerDst] = rx_set_vec_f128(0x41937f76fede16ee, 0x411b414296ce93b6);
		reg.r[registerSrc] = 0xFFFFFFFFFFFFE930;
		rx_set_rounding_mode(RoundDown);
		decoder.executeInstruction(ibc, pc, (uint8_t*)&mockScratchpad, config);
		rx_store_vec_f128((double*)&vec, reg.e[registerDst]);
		assert(equalsHex(&vec, "e6b269639484434632474a66635ba547"));
	});

	runTest("FDIV_M RoundUp (execute)", RANDOMX_FREQ_FDIV_M > 0, [&] {
		alignas(16) uint64_t vec[2];
		alignas(16) uint32_t mockScratchpad[2];
		store32(&mockScratchpad[0], 0xd350a1b6);
		store32(&mockScratchpad[1], 0x8b2460d9);
		store64(&config.eMask[0], 0x3a0000000005d11a);
		store64(&config.eMask[1], 0x39000000001ba31e);
		reg.e[registerDst] = rx_set_vec_f128(0x41937f76fede16ee, 0x411b414296ce93b6);
		reg.r[registerSrc] = 0xFFFFFFFFFFFFE930;
		rx_set_rounding_mode(RoundUp);
		decoder.executeInstruction(ibc, pc, (uint8_t*)&mockScratchpad, config);
		rx_store_vec_f128((double*)&vec, reg.e[registerDst]);
		assert(equalsHex(&vec, "e7b269639484434633474a66635ba547"));
	});

	runTest("FSQRT_R (decode)", RANDOMX_FREQ_FSQRT_R > 0, [&] {
		randomx::Instruction instr;
		instr.opcode = randomx::ceil_FSQRT_R - 1;
		instr.dst = registerHigh | registerDst;
		decoder.compileInstruction(instr, pc, ibc);
		assert(ibc.type == randomx::InstructionType::FSQRT_R);
		assert(ibc.fdst == &reg.e[registerDst]);
	});

	runTest("FSQRT_R RoundToNearest (execute)", RANDOMX_FREQ_FSQRT_R > 0, [&] {
		alignas(16) uint64_t vec[2];
		reg.e[registerDst] = rx_set_vec_f128(0x41b6b21c11affea7, 0x40526a7e778d9824);
		rx_set_rounding_mode(RoundToNearest);
		decoder.executeInstruction(ibc, pc, nullptr, config);
		rx_store_vec_f128((double*)&vec, reg.e[registerDst]);
		assert(equalsHex(&vec, "e81f300b612a21408dbaa33f570ed340"));
	});

	runTest("FSQRT_R RoundDown/RoundToZero (execute)", RANDOMX_FREQ_FSQRT_R > 0, [&] {
		alignas(16) uint64_t vec[2];
		reg.e[registerDst] = rx_set_vec_f128(0x41b6b21c11affea7, 0x40526a7e778d9824);
		rx_set_rounding_mode(RoundDown);
		decoder.executeInstruction(ibc, pc, nullptr, config);
		rx_store_vec_f128((double*)&vec, reg.e[registerDst]);
		assert(equalsHex(&vec, "e81f300b612a21408cbaa33f570ed340"));
	});

	runTest("FSQRT_R RoundUp (execute)", RANDOMX_FREQ_FSQRT_R > 0, [&] {
		alignas(16) uint64_t vec[2];
		reg.e[registerDst] = rx_set_vec_f128(0x41b6b21c11affea7, 0x40526a7e778d9824);
		rx_set_rounding_mode(RoundUp);
		decoder.executeInstruction(ibc, pc, nullptr, config);
		rx_store_vec_f128((double*)&vec, reg.e[registerDst]);
		assert(equalsHex(&vec, "e91f300b612a21408dbaa33f570ed340"));
	});

	runTest("CBRANCH (decode) 100", RANDOMX_FREQ_CBRANCH > 0, [&] {
		randomx::Instruction instr;
		instr.opcode = randomx::ceil_CBRANCH - 1;
		instr.dst = registerHigh | registerDst;
		instr.setImm32(imm32);
		instr.mod = 48;
		decoder.compileInstruction(instr, 100, ibc);
		assert(ibc.type == randomx::InstructionType::CBRANCH);
		assert(ibc.idst == &reg.r[registerDst]);
		assert(ibc.imm == 0xFFFFFFFFC0CB9AD2);
		assert(ibc.memMask == 0x7F800);
		assert(ibc.target == pc);
	});

	runTest("CBRANCH (decode) 200", RANDOMX_FREQ_CBRANCH > 0, [&] {
		randomx::Instruction instr;
		instr.opcode = randomx::ceil_CBRANCH - 1;
		instr.dst = registerHigh | registerDst;
		instr.setImm32(imm32);
		instr.mod = 48;
		decoder.compileInstruction(instr, pc = 200, ibc);
		assert(ibc.type == randomx::InstructionType::CBRANCH);
		assert(ibc.idst == &reg.r[registerDst]);
		assert(ibc.imm == 0xFFFFFFFFC0CB9AD2);
		assert(ibc.memMask == 0x7F800);
		assert(ibc.target == 100);
	});

	runTest("CBRANCH not taken (execute)", RANDOMX_FREQ_CBRANCH > 0, [&] {
		reg.r[registerDst] = 0;
		decoder.executeInstruction(ibc, pc, nullptr, config);
		assert(pc == 200);
	});

	runTest("CBRANCH taken (execute)", RANDOMX_FREQ_CBRANCH > 0, [&] {
		reg.r[registerDst] = 0xFFFFFFFFFFFC6800;
		decoder.executeInstruction(ibc, pc, nullptr, config);
		assert(pc == ibc.target);
	});

	runTest("CFROUND (decode)", RANDOMX_FREQ_CFROUND > 0, [&] {
		randomx::Instruction instr;
		instr.opcode = randomx::ceil_CFROUND - 1;
		instr.src = registerHigh | registerSrc;
		instr.setImm32(imm32);
		decoder.compileInstruction(instr, 100, ibc);
		assert(ibc.type == randomx::InstructionType::CFROUND);
		assert(ibc.isrc == &reg.r[registerSrc]);
		assert(ibc.imm == 18);
	});

	runTest("ISTORE L1 (decode)", RANDOMX_FREQ_ISTORE > 0, [&] {
		randomx::Instruction instr;
		instr.opcode = randomx::ceil_ISTORE - 1;
		instr.src = registerHigh | registerSrc;
		instr.dst = registerHigh | registerDst;
		instr.setImm32(imm32);
		instr.mod = 1;
		decoder.compileInstruction(instr, pc, ibc);
		assert(ibc.type == randomx::InstructionType::ISTORE);
		assert(ibc.idst == &reg.r[registerDst]);
		assert(ibc.isrc == &reg.r[registerSrc]);
		assert(ibc.imm == imm64);
		assert(ibc.memMask == randomx::ScratchpadL1Mask);
	});

	runTest("ISTORE L2 (decode)", RANDOMX_FREQ_ISTORE > 0, [&] {
		randomx::Instruction instr;
		instr.opcode = randomx::ceil_ISTORE - 1;
		instr.src = registerHigh | registerSrc;
		instr.dst = registerHigh | registerDst;
		instr.setImm32(imm32);
		instr.mod = 0;
		decoder.compileInstruction(instr, pc, ibc);
		assert(ibc.type == randomx::InstructionType::ISTORE);
		assert(ibc.idst == &reg.r[registerDst]);
		assert(ibc.isrc == &reg.r[registerSrc]);
		assert(ibc.imm == imm64);
		assert(ibc.memMask == randomx::ScratchpadL2Mask);
	});

	runTest("ISTORE L3 (decode)", RANDOMX_FREQ_ISTORE > 0, [&] {
		randomx::Instruction instr;
		instr.opcode = randomx::ceil_ISTORE - 1;
		instr.src = registerHigh | registerSrc;
		instr.dst = registerHigh | registerDst;
		instr.setImm32(imm32);
		instr.mod = 224;
		decoder.compileInstruction(instr, pc, ibc);
		assert(ibc.type == randomx::InstructionType::ISTORE);
		assert(ibc.idst == &reg.r[registerDst]);
		assert(ibc.isrc == &reg.r[registerSrc]);
		assert(ibc.imm == imm64);
		assert(ibc.memMask == randomx::ScratchpadL3Mask);
	});

#ifdef RANDOMX_FORCE_SECURE
	vm = randomx_create_vm(RANDOMX_FLAG_DEFAULT | RANDOMX_FLAG_SECURE, cache, nullptr);
#else
	vm = randomx_create_vm(RANDOMX_FLAG_DEFAULT, cache, nullptr);
#endif

	auto test_a = [&] {
		char hash[RANDOMX_HASH_SIZE];
		calcStringHash("test key 000", "This is a test", &hash);
		assert(equalsHex(hash, "639183aae1bf4c9a35884cb46b09cad9175f04efd7684e7262a0ac1c2f0b4e3f"));
	};

	auto test_b = [&] {
		char hash[RANDOMX_HASH_SIZE];
		calcStringHash("test key 000", "Lorem ipsum dolor sit amet", &hash);
		assert(equalsHex(hash, "300a0adb47603dedb42228ccb2b211104f4da45af709cd7547cd049e9489c969"));
	};

	auto test_c = [&] {
		char hash[RANDOMX_HASH_SIZE];
		calcStringHash("test key 000", "sed do eiusmod tempor incididunt ut labore et dolore magna aliqua", &hash);
		assert(equalsHex(hash, "c36d4ed4191e617309867ed66a443be4075014e2b061bcdaf9ce7b721d2b77a8"));
	};

	auto test_d = [&] {
		char hash[RANDOMX_HASH_SIZE];
		calcStringHash("test key 001", "sed do eiusmod tempor incididunt ut labore et dolore magna aliqua", &hash);
		assert(equalsHex(hash, "e9ff4503201c0c2cca26d285c93ae883f9b1d30c9eb240b820756f2d5a7905fc"));
	};

	auto test_e = [&] {
		char hash[RANDOMX_HASH_SIZE];
		calcHexHash("test key 001", "0b0b98bea7e805e0010a2126d287a2a0cc833d312cb786385a7c2f9de69d25537f584a9bc9977b00000000666fd8753bf61a8631f12984e3fd44f4014eca629276817b56f32e9b68bd82f416", &hash);
		//std::cout << std::endl;
		//outputHex(std::cout, (const char*)hash, sizeof(hash));
		//std::cout << std::endl;
		assert(equalsHex(hash, "c56414121acda1713c2f2a819d8ae38aed7c80c35c2a769298d34f03833cd5f1"));
	};

	runTest("Hash test 1a (interpreter)", stringsEqual(RANDOMX_ARGON_SALT, "RandomX\x03"), test_a);

	runTest("Hash test 1b (interpreter)", stringsEqual(RANDOMX_ARGON_SALT, "RandomX\x03"), test_b);

	runTest("Hash test 1c (interpreter)", stringsEqual(RANDOMX_ARGON_SALT, "RandomX\x03"), test_c);

	runTest("Hash test 1d (interpreter)", stringsEqual(RANDOMX_ARGON_SALT, "RandomX\x03"), test_d);

	runTest("Hash test 1e (interpreter)", stringsEqual(RANDOMX_ARGON_SALT, "RandomX\x03"), test_e);

	if (RANDOMX_HAVE_COMPILER) {
		randomx_release_cache(cache);
		randomx_destroy_vm(vm);
		vm = nullptr;
		cache = randomx_alloc_cache(RANDOMX_FLAG_JIT);
		initCache("test key 000");
#ifdef RANDOMX_FORCE_SECURE
		vm = randomx_create_vm(RANDOMX_FLAG_JIT | RANDOMX_FLAG_SECURE, cache, nullptr);
#else
		vm = randomx_create_vm(RANDOMX_FLAG_JIT, cache, nullptr);
#endif
	}

	runTest("Hash test 2a (compiler)", RANDOMX_HAVE_COMPILER && stringsEqual(RANDOMX_ARGON_SALT, "RandomX\x03"), test_a);

	runTest("Hash test 2b (compiler)", RANDOMX_HAVE_COMPILER && stringsEqual(RANDOMX_ARGON_SALT, "RandomX\x03"), test_b);

	runTest("Hash test 2c (compiler)", RANDOMX_HAVE_COMPILER && stringsEqual(RANDOMX_ARGON_SALT, "RandomX\x03"), test_c);

	runTest("Hash test 2d (compiler)", RANDOMX_HAVE_COMPILER && stringsEqual(RANDOMX_ARGON_SALT, "RandomX\x03"), test_d);

	runTest("Hash test 2e (compiler)", RANDOMX_HAVE_COMPILER && stringsEqual(RANDOMX_ARGON_SALT, "RandomX\x03"), test_e);

	auto flags = randomx_get_flags();

	randomx_release_cache(cache);
	cache = randomx_alloc_cache(RANDOMX_FLAG_ARGON2_SSSE3);

	runTest("Cache initialization: SSSE3", (flags & RANDOMX_FLAG_ARGON2_SSSE3) && RANDOMX_ARGON_ITERATIONS == 3 && RANDOMX_ARGON_LANES == 1 && RANDOMX_ARGON_MEMORY == 262144 && stringsEqual(RANDOMX_ARGON_SALT, "RandomX\x03"), []() {
		initCache("test key 000");
		uint64_t* cacheMemory = (uint64_t*)cache->memory;
		assert(cacheMemory[0] == 0x191e0e1d23c02186);
		assert(cacheMemory[1568413] == 0xf1b62fe6210bf8b1);
		assert(cacheMemory[33554431] == 0x1f47f056d05cd99b);
	});

	if (cache != nullptr)
		randomx_release_cache(cache);
	cache = randomx_alloc_cache(RANDOMX_FLAG_ARGON2_AVX2);

	runTest("Cache initialization: AVX2", (flags & RANDOMX_FLAG_ARGON2_AVX2) && RANDOMX_ARGON_ITERATIONS == 3 && RANDOMX_ARGON_LANES == 1 && RANDOMX_ARGON_MEMORY == 262144 && stringsEqual(RANDOMX_ARGON_SALT, "RandomX\x03"), []() {
		initCache("test key 000");
		uint64_t* cacheMemory = (uint64_t*)cache->memory;
		assert(cacheMemory[0] == 0x191e0e1d23c02186);
		assert(cacheMemory[1568413] == 0xf1b62fe6210bf8b1);
		assert(cacheMemory[33554431] == 0x1f47f056d05cd99b);
	});

	if (cache != nullptr)
		randomx_release_cache(cache);
	cache = randomx_alloc_cache(RANDOMX_FLAG_DEFAULT);

	runTest("Hash batch test", RANDOMX_HAVE_COMPILER && stringsEqual(RANDOMX_ARGON_SALT, "RandomX\x03"), []() {
		char hash1[RANDOMX_HASH_SIZE];
		char hash2[RANDOMX_HASH_SIZE];
		char hash3[RANDOMX_HASH_SIZE];
		initCache("test key 000");
		char input1[] = "This is a test";
		char input2[] = "Lorem ipsum dolor sit amet";
		char input3[] = "sed do eiusmod tempor incididunt ut labore et dolore magna aliqua";

		randomx_calculate_hash_first(vm, input1, sizeof(input1) - 1);
		randomx_calculate_hash_next(vm, input2, sizeof(input2) - 1, &hash1);
		randomx_calculate_hash_next(vm, input3, sizeof(input3) - 1, &hash2);
		randomx_calculate_hash_last(vm, &hash3);

		assert(equalsHex(hash1, "639183aae1bf4c9a35884cb46b09cad9175f04efd7684e7262a0ac1c2f0b4e3f"));
		assert(equalsHex(hash2, "300a0adb47603dedb42228ccb2b211104f4da45af709cd7547cd049e9489c969"));
		assert(equalsHex(hash3, "c36d4ed4191e617309867ed66a443be4075014e2b061bcdaf9ce7b721d2b77a8"));
	});

	runTest("Preserve rounding mode", RANDOMX_FREQ_CFROUND > 0, []() {
		rx_set_rounding_mode(RoundToNearest);
		char hash[RANDOMX_HASH_SIZE];
		calcStringHash("test key 000", "Lorem ipsum dolor sit amet", &hash);
		assert(equalsHex(hash, "300a0adb47603dedb42228ccb2b211104f4da45af709cd7547cd049e9489c969"));
		assert(rx_get_rounding_mode() == RoundToNearest);
	});

	randomx_destroy_vm(vm);
	vm = nullptr;

	if (cache != nullptr)
		randomx_release_cache(cache);

	std::cout << std::endl << "All tests PASSED" << std::endl;

	if (skipped) {
		std::cout << skipped << " tests were SKIPPED due to incompatible configuration (see above)" << std::endl;
	}
}
