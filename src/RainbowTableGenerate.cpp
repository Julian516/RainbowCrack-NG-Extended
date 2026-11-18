/*
   RainbowCrack - a general propose implementation of Philippe Oechslin's faster time-memory trade-off technique.

   Copyright (C) Zhu Shuanglei <shuanglei@hotmail.com>
*/

#ifdef _WIN32
	#pragma warning(disable : 4786)
#endif

#ifdef _WIN32
	#include <windows.h>
#else
	#include <unistd.h>
#endif
#include <time.h>

#include <cstring>
#include <algorithm>
#include "ChainWalkContext.h"

#include <thread>
#include <atomic>
#include <vector>
#include <chrono>
#include <algorithm>
#include <cerrno>
#include <climits>
#include <cstdlib>
#include <cstdint>
#include <numeric>
#include <stdexcept>

void Usage()
{
	Logo();

	printf("usage: rtgen hash_algorithm \\\n");
	printf("             plain_charset plain_len_min plain_len_max \\\n");
	printf("             rainbow_table_index \\\n");
	printf("             rainbow_chain_length rainbow_chain_count \\\n");
	printf("             file_title_suffix\n");
	printf("       rtgen hash_algorithm \\\n");
	printf("             plain_charset plain_len_min plain_len_max \\\n");
	printf("             rainbow_table_index \\\n");
	printf("             -bench\n");
	printf("\n");

	CHashRoutine hr;
	printf("hash_algorithm:       available: %s\n", hr.GetAllHashRoutineName().c_str());
	printf("plain_charset:        use any charset name in charset.txt here\n");
	printf("                      use \"byte\" to specify all 256 characters as the charset of the plaintext\n");
	printf("plain_len_min:        min length of the plaintext\n");
	printf("plain_len_max:        max length of the plaintext\n");
	printf("rainbow_table_index:  index of the rainbow table\n");
	printf("rainbow_chain_length: length of the rainbow chain\n");
	printf("rainbow_chain_count:  count of the rainbow chain to generate\n");
	printf("file_title_suffix:    the string appended to the file title\n");
	printf("                      add your comment of the generated rainbow table here\n");
	printf("-bench:               do some benchmark\n");

	printf("\n");
	printf("example: rtgen lm alpha 1 7 0 100 16 test\n");
	printf("         rtgen md5 byte 4 4 0 100 16 test\n");
	printf("         rtgen sha1 numeric 1 10 0 100 16 test\n");
	printf("         rtgen lm alpha 1 7 0 -bench\n");
}

void Bench(string sHashRoutineName, string sCharsetName, int nPlainLenMin, int nPlainLenMax, int nRainbowTableIndex)
{
	// Setup CChainWalkContext
	if (!CChainWalkContext::SetHashRoutine(sHashRoutineName))
	{
		printf("hash routine %s not supported\n", sHashRoutineName.c_str());
		return;
	}
	if (!CChainWalkContext::SetPlainCharset(sCharsetName, nPlainLenMin, nPlainLenMax))
		return;
	if (!CChainWalkContext::SetRainbowTableIndex(nRainbowTableIndex))
	{
		printf("invalid rainbow table index %d\n", nRainbowTableIndex);
		return;
	}

	// Bench hash
	{
	CChainWalkContext cwc;
	cwc.GenerateRandomIndex();
	cwc.IndexToPlain();

	clock_t t1 = clock();
	int nLoop = 2500000;
	int i;
	for (i = 0; i < nLoop; i++)
		cwc.PlainToHash();
	clock_t t2 = clock();
	float fTime = 1.0f * (t2 - t1) / CLOCKS_PER_SEC;

	printf("%s hash speed: %d / s\n", sHashRoutineName.c_str(), int(nLoop / fTime));
	}

	// Bench step
	{
	CChainWalkContext cwc;
	cwc.GenerateRandomIndex();

	clock_t t1 = clock();
	int nLoop = 2500000;
	int i;
	for (i = 0; i < nLoop; i++)
	{
		cwc.IndexToPlain();
		cwc.PlainToHash();
		cwc.HashToIndex(i);
	}
	clock_t t2 = clock();
	float fTime = 1.0f * (t2 - t1) / CLOCKS_PER_SEC;

	printf("%s step speed: %d / s\n", sHashRoutineName.c_str(), int(nLoop / fTime));
	}
}

static uint64_t deterministicStart(
	uint64_t chainId,
	uint64_t seed,
	uint64_t space
) {
	if (space == 0 || chainId >= space)
		throw std::out_of_range("Invalid chain index");

	uint64_t offset = seed % space;
	uint64_t stride = (seed % (space - 1)) + 1;

	while (std::gcd(stride, space) != 1) {
		stride = stride % (space - 1) + 1;
	}

	return static_cast<uint64_t>(
		(static_cast<__uint128_t>(chainId) * stride
			+ offset) % space
		);
}



int main(int argc, char* argv[])
{
	if (argc == 7)
	{
		if (strcmp(argv[6], "-bench") == 0)
		{
			Bench(argv[1], argv[2], atoi(argv[3]), atoi(argv[4]), atoi(argv[5]));
			return 0;
		}
	}

	if (argc != 9 && argc != 11)
	{
		Usage();
		return 0;
	}

	int threadCount = 1;

	if (argc == 11) {
		if (strcmp(argv[9], "--threads") != 0) {
			Usage();
			return 1;
		}

		errno = 0;
		char* end = nullptr;
		long value = strtol(argv[10], &end, 10);

		if (errno != 0 || end == argv[10] || *end != '\0'
			|| value < 1 || value > 64) {
			printf("Invalid thread count (1-64)\n");
			return 1;
		}

		threadCount = static_cast<int>(value);
	}

	printf("Threads: %d\n", threadCount);

	string sHashRoutineName  = argv[1];
	string sCharsetName      = argv[2];
	int nPlainLenMin         = atoi(argv[3]);
	int nPlainLenMax         = atoi(argv[4]);
	int nRainbowTableIndex   = atoi(argv[5]);

	int nRainbowChainLen     = atoi(argv[6]);
	int nRainbowChainCount   = atoi(argv[7]);
	string sFileTitleSuffix  = argv[8];

	// nRainbowChainCount check
	if (nRainbowChainCount >= 134217728)
	{
		printf("this will generate a table larger than 2GB, which is not supported\n");
		printf("please use a smaller rainbow_chain_count(less than 134217728)\n");
		return 0;
	}

	// Setup CChainWalkContext
	if (!CChainWalkContext::SetHashRoutine(sHashRoutineName))
	{
		printf("hash routine %s not supported\n", sHashRoutineName.c_str());
		return 0;
	}
	if (!CChainWalkContext::SetPlainCharset(sCharsetName, nPlainLenMin, nPlainLenMax))
		return 0;
	if (!CChainWalkContext::SetRainbowTableIndex(nRainbowTableIndex))
	{
		printf("invalid rainbow table index %d\n", nRainbowTableIndex);
		return 0;
	}
	CChainWalkContext::Dump();

	// Low priority
#ifdef _WIN32
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_IDLE);
#else
	nice(19);
#endif

	// FileName
	char szFileName[256];
	sprintf(szFileName, "%s_%s#%d-%d_%d_%dx%d_%s.rt", sHashRoutineName.c_str(),
													  sCharsetName.c_str(),
													  nPlainLenMin,
													  nPlainLenMax,
													  nRainbowTableIndex,
													  nRainbowChainLen,
													  nRainbowChainCount,
													  sFileTitleSuffix.c_str());

	// Open file
	fclose(fopen(szFileName, "a"));
	FILE* file = fopen(szFileName, "r+b");
	if (file == NULL)
	{
		printf("failed to create %s\n", szFileName);
		return 0;
	}

	// Check existing chains
	unsigned int nDataLen = GetFileLen(file);
	nDataLen = nDataLen / 16 * 16;
	if (nDataLen == nRainbowChainCount * 16)
	{
		printf("precomputation of this rainbow table already finished\n");
		fclose(file);
		return 0;
	}
	if (nDataLen > 0)
		printf("continuing from interrupted precomputation...\n");
	fseek(file, nDataLen, SEEK_SET);

	// Generate rainbow table -  multithreaded

	struct ChainRecord {
		uint64 start;
		uint64 end;
	};

	constexpr int BATCH_SIZE = 1024;

	printf("generating with %d threads...\n", threadCount);

	auto t0 = std::chrono::steady_clock::now();

	for (int base = nDataLen / 16;
		base < nRainbowChainCount;
		base += BATCH_SIZE)
	{
		const int count = std::min(
			BATCH_SIZE, nRainbowChainCount - base
		);

		std::vector<ChainRecord> records(count);
		std::atomic<int> next{ 0 };

		auto worker = [&]() {
			CChainWalkContext cwc;

			while (true) {
				int slot = next.fetch_add(1);

				if (slot >= count)
					break;

				constexpr uint64_t SEED = 20261118ULL;

				const uint64_t chainId =
					static_cast<uint64_t>(base) + slot;

				const uint64_t start = deterministicStart(
					chainId,
					SEED,
					CChainWalkContext::GetPlainSpaceTotal()
				);

				cwc.SetIndex(start);
				records[slot].start = start;

				for (int pos = 0;
					pos < nRainbowChainLen - 1;
					++pos)
				{
					cwc.IndexToPlain();
					cwc.PlainToHash();
					cwc.HashToIndex(pos);
				}

				records[slot].end = cwc.GetIndex();
			}
			};

		std::vector<std::thread> threads;
		threads.reserve(threadCount);

		for (int t = 0; t < threadCount; ++t)
			threads.emplace_back(worker);

		for (auto& t : threads)
			t.join();

		// Single writer: preserve the legacy .rt format
		for (const auto& record : records) {
			if (fwrite(&record.start, 1, 8, file) != 8 ||
				fwrite(&record.end, 1, 8, file) != 8) {
				fprintf(stderr, "disk write failure\n");
				fclose(file);
				return 1;
			}
		}

		int completed = base + count;

		auto elapsed = std::chrono::duration<double>(
			std::chrono::steady_clock::now() - t0
		).count();

		printf(
			"\r%d/%d chains | %.2f chains/s",
			completed,
			nRainbowChainCount,
			(completed - static_cast<int>(nDataLen / 16))
			/ std::max(elapsed, 1e-9)
		);

		fflush(stdout);
	}

	printf("\n");
	// Close
	fclose(file);

	return 0;
}
