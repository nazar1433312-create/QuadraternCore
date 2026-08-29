# testnet-2 implementation — progress snapshot

testnet-1's progress log is frozen at `PROGRESS.md` (that milestone is complete — see `ROADMAP.md`). This file tracks testnet-2: multi-algo consensus (ProgPoW/RandomX alongside SHA-256d), LWMA per-algo difficulty, dynamic-weight fork-choice, and the two-axis anti-spam filter — see `ROADMAP.md`'s testnet-2 section for the target scope.

## Готово, закоммичено и проверено сборкой + тестами

1. **`src/qtrn/pow_algo.h` / `.cpp`** — which of the three algorithms mined a block is encoded in `nVersion` bits 8-9 (`PowAlgo::SHA256D/PROGPOW/RANDOMX`), not a new header field — keeps the 80-byte header format stable, same reasoning as SegWit's own version-bit signaling (real precedent for algo-in-version encoding: Myriadcoin, early Verge). Bits chosen to avoid every BIP9 deployment bit already claimed in this tree (`DEPLOYMENT_TAPROOT=2`, `DEPLOYMENT_MWEB=4`, `DEPLOYMENT_TESTDUMMY=28`) and the top-3-bit `0x20000000` BIP9 marker pattern — `GetPowAlgo`/`SetPowAlgo` never disturb unrelated version bits.
2. **`src/test/qtrn_pow_algo_tests.cpp`** — 5 юнит-тестов: round-trip всех трёх значений, сохранность BIP9-маркера и посторонних deployment-битов при кодировании, перезапись предыдущего значения алгоритма, обнаружение неприсвоенного 4-го значения (2 бита кодируют 4 значения, используется 3), корректные короткие имена для логов/RPC.

**Все 48 тестов (43 из testnet-1 + 5 новых) прошли: `Running 48 test cases... *** No errors detected`.**

## Начато, не закончено

Это самое начало testnet-2 — зафиксирован только формат кодирования алгоритма в блоке. Ничего из следующего ещё не начато:
- `GetPoWHash()` пока всегда считает SHA256d независимо от `PowAlgo` — не диспетчеризирует на ProgPoW/RandomX.
- ProgPoW и RandomX как таковые не подключены к дереву вообще — нужно вендорить (RandomX — библиотека Monero, ProgPoW — референс из Ravencoin/Firo) и завести под них сборку.
- LWMA per-algo difficulty retargeting (окно 120 блоков, целевой интервал 3 минуты на алгоритм, клэмп +15%/−10%) — не начато.
- Fork-choice на основе реальной сложности алгоритма (не плоская константа) — не начато.
- Tie-break/орфан-логика для параллельных гонок — не начато.
- Анти-спам P2P-фильтр (rate-limit по источнику отдельно от локального сравнения весов) — не начато.
- `chainparams.cpp` (`consensus.powLimit` и связанные параметры) сейчас рассчитаны на один алгоритм — нужно решить, отдельный `powLimit`/сложность на каждый алгоритм или общая структура.

## Следующие шаги при возобновлении

1. **Решить, как вендорить RandomX и ProgPoW** — вероятно, самый крупный и рискованный кусок testnet-2 (реальные внешние C/C++ библиотеки, свои системы сборки, нужно завести в наш `Makefile.am`/autotools). Стоит обсудить подход явно перед тем, как тянуть исходники — большой объём, ошибиться дорого.
2. Расширить `Consensus::Params` под сложность/таргет отдельно на каждый алгоритм (сейчас там один `powLimit`/`nPowTargetSpacing` на всю цепь).
3. Реализовать LWMA retargeting как отдельный, изолированный, тестируемый модуль (по образцу того, как делали `stake_*` модули в testnet-1) — до подключения к `pow.cpp`/`validation.cpp`.
4. `GetPoWHash()` — диспетчеризация по `qtrn::GetPowAlgo(nVersion)` на нужный алгоритм.
5. Fork-choice: заменить сравнение по высоте/накопленной работе на версию, учитывающую реальную сложность каждого алгоритма — искать все места, где сейчас используется `nChainWork`/`GetBlockProof`, чтобы не пропустить нужную точку подключения.
6. Проверять сборкой + тестами после каждого шага, не совмещая правки с работающей фоновой сборкой (см. полный список уроков в `PROGRESS.md`).
