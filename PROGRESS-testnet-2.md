# testnet-2 implementation — progress snapshot

testnet-1's progress log is frozen at `PROGRESS.md` (that milestone is complete — see `ROADMAP.md`). This file tracks testnet-2: multi-algo consensus (ProgPoW/RandomX alongside SHA-256d), LWMA per-algo difficulty, dynamic-weight fork-choice, and the two-axis anti-spam filter — see `ROADMAP.md`'s testnet-2 section for the target scope.

## Готово, закоммичено и проверено сборкой + тестами

1. **`src/qtrn/pow_algo.h` / `.cpp`** — which of the three algorithms mined a block is encoded in `nVersion` bits 8-9 (`PowAlgo::SHA256D/PROGPOW/RANDOMX`), not a new header field — keeps the 80-byte header format stable, same reasoning as SegWit's own version-bit signaling (real precedent for algo-in-version encoding: Myriadcoin, early Verge). Bits chosen to avoid every BIP9 deployment bit already claimed in this tree (`DEPLOYMENT_TAPROOT=2`, `DEPLOYMENT_MWEB=4`, `DEPLOYMENT_TESTDUMMY=28`) and the top-3-bit `0x20000000` BIP9 marker pattern — `GetPowAlgo`/`SetPowAlgo` never disturb unrelated version bits.
2. **`src/test/qtrn_pow_algo_tests.cpp`** — 5 юнит-тестов: round-trip всех трёх значений, сохранность BIP9-маркера и посторонних deployment-битов при кодировании, перезапись предыдущего значения алгоритма, обнаружение неприсвоенного 4-го значения (2 бита кодируют 4 значения, используется 3), корректные короткие имена для логов/RPC.

### Решение по архитектуре: одна цепь, не три отдельные

Явно обсудили и подтвердили: три алгоритма — это **три канала, претендующие на один и тот же следующий блок одной общей цепи** (как и было в исходной спеке — "три параллельные гонки на один слот"), а не три независимых блокчейна с мостом между ними. Один UTXO-набор, один баланс на адрес, одна эмиссия. Вариант с тремя отдельными связанными цепями рассматривался и отклонён — потребовал бы моста и сломал бы уже сделанную работу над genesis-стейкерами/эмиссией, которая построена на "один счётчик блоков одной цепи".

3. **`src/qtrn/algo_hash.h` / `.cpp`** — `ComputeAlgoHash()`: диспетчеризация хэша PoW по алгоритму блока. SHA256D — настоящая, финальная реализация (тот же `SerializeHash`, что и раньше). **ProgPoW и RandomX — осознанные временные заглушки** (доменно-разделённый хэш с фиксированным префиксом на каждый алгоритм, НЕ настоящие алгоритмы) — чтобы построить и проверить diff retargeting/fork-choice/tie-break без риска сразу вендорить обе крупные чужие библиотеки. `IsAlgoHashReal()` явно помечает, какие алгоритмы настоящие, а какие ещё заглушки — жёсткое требование: заглушка не должна попасть ни в genesis, ни в тестнет, ни тем более в mainnet.
4. **`src/test/qtrn_algo_hash_tests.cpp`** — 6 тестов: SHA256D совпадает с обычным `GetHash()`, детерминированность на каждый алгоритм, разные алгоритмы никогда не коллизируют на одинаковом содержимом заголовка, отражает реальные изменения контента, неприсвоенный algo-id даёт null-хэш, только SHA256D помечен как настоящий.

**Все 54 теста прошли: `Running 54 test cases... *** No errors detected`.**

5. **`src/primitives/block.cpp`** — `CBlockHeader::GetPoWHash()` теперь реально зовёт `qtrn::ComputeAlgoHash(*this)` вместо прямого `SerializeHash` — диспетчеризация подключена к реальному пути, не просто протестирована изолированно.

### Найден и исправлен реальный риск: неверный слой библиотек

`primitives/block.cpp` — часть `libbitcoin_consensus_a` (минимальная база, от которой также собирается `libbitcoinconsensus.la` — внешняя разделяемая библиотека для сторонних потребителей, независимая от полного узла). Я по инерции добавил `qtrn/pow_algo.*`/`qtrn/algo_hash.*` в `libbitcoin_server_a_SOURCES` (как остальные `qtrn/*` модули) — но раз `block.cpp` теперь их вызывает, а `block.cpp` живёт слоем НИЖЕ сервера, это сломало бы линковку `libbitcoinconsensus.la` и любых автономных утилит, использующих только консенсус-библиотеку (пропущенный символ). **Исправлено:** оба модуля перенесены в `libbitcoin_consensus_a_SOURCES` (их зависимости — `hash.h`, `primitives/block.h` — и так уже там). Отдельно перепроверено сборкой именно таргета `libbitcoinconsensus.la` — линкуется чисто.

**Урок:** при добавлении вызова из файла в `libbitcoin_consensus_a`/`libbitcoin_common_a` на что-то новое — сверять, в каком именно `_SOURCES`-списке лежит вызываемый код, а не класть новый код по умолчанию туда же, где лежали предыдущие (не консенсус-критичные) `qtrn/*` модули.

## Начато, не закончено

- ProgPoW и RandomX как таковые (настоящие библиотеки) не подключены к дереву вообще — вендоринг сознательно отложен, сейчас работаем на заглушках (`algo_hash.h`).
- LWMA per-algo difficulty retargeting (окно 120 блоков, целевой интервал 3 минуты на алгоритм, клэмп +15%/−10%) — не начато.
- Fork-choice на основе реальной сложности алгоритма (не плоская константа) — не начато.
- Tie-break/орфан-логика для параллельных гонок — не начато.
- Анти-спам P2P-фильтр (rate-limit по источнику отдельно от локального сравнения весов) — не начато.
- `chainparams.cpp` (`consensus.powLimit` и связанные параметры) сейчас рассчитаны на один алгоритм — нужно решить, отдельный `powLimit`/сложность на каждый алгоритм или общая структура.

## Следующие шаги при возобновлении

1. Расширить `Consensus::Params` под сложность/таргет отдельно на каждый алгоритм (сейчас там один `powLimit`/`nPowTargetSpacing` на всю цепь).
2. Реализовать LWMA retargeting как отдельный, изолированный, тестируемый модуль (по образцу того, как делали `stake_*` модули в testnet-1) — до подключения к `pow.cpp`/`validation.cpp`.
3. Fork-choice: заменить сравнение по высоте/накопленной работе на версию, учитывающую реальную сложность каждого алгоритма — искать все места, где сейчас используется `nChainWork`/`GetBlockProof`, чтобы не пропустить нужную точку подключения.
4. Когда дойдём до реального вендоринга RandomX/ProgPoW — заменить только тела двух функций-заглушек в `algo_hash.cpp`, остальной код (diff retargeting, fork-choice) трогать не придётся, если контракт `ComputeAlgoHash()` не менять.
5. Проверять сборкой + тестами после каждого шага, не совмещая правки с работающей фоновой сборкой (см. полный список уроков в `PROGRESS.md`).
6. При добавлении вызова из низкоуровневого файла (`libbitcoin_consensus`/`libbitcoin_common`) на новый код — сверять, в каком `_SOURCES`-списке реально лежит этот код, не класть по умолчанию туда же, где лежали предыдущие модули.
