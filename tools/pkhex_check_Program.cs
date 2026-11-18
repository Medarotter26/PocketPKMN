// pkhex-check: PKHeX.Core legality checks for PocketPKMN's generated catches (PC tests only).
//   pkhex-check sav <save file>          - every boxed Pokemon of a real save (smoke test of the checker)
//   pkhex-check pk <gen> <file> [...]    - raw entity files (gen 2 = PK2 with names, gen 6 = PK6 box data)
// Prints one line per Pokemon: OK / ILLEGAL + species, then the report lines for illegal ones. Exit code 1 if any
// Pokemon is illegal.
using PKHeX.Core;

static int Check(PKM pk, string label, bool verbose)
{
    var la = new LegalityAnalysis(pk);
    var name = GameInfo.Strings.Species[pk.Species];
    Console.WriteLine($"{(la.Valid ? "OK     " : "ILLEGAL")} {label}: {name} Lv{pk.CurrentLevel} met {pk.MetLevel} " +
                      $"loc {pk.MetLocation} egg {pk.EggLocation} ability {pk.Ability}/{pk.AbilityNumber}");
    if (!la.Valid || verbose)
        foreach (var line in la.Report().Split('\n'))
            if (line.Trim().Length > 0)
                Console.WriteLine("    " + line.Trim());
    return la.Valid ? 0 : 1;
}

static SAV7 Blank7(string version) // the blank Sun / Moon / Ultra test save (also the trainer of "pkctx")
{
    SAV7 sav = version is "US" or "UM" ? new SAV7USUM() : new SAV7SM();
    sav.Version = Enum.Parse<GameVersion>(version);
    sav.OT = "TESTER";
    sav.TID16 = 24680;
    sav.SID16 = 13579;
    sav.Gender = 1;
    sav.Language = (int)LanguageID.English;
    sav.Country = 49;
    sav.Region = 7;
    sav.ConsoleRegion = 1;
    return sav;
}

// v0.94 (FRLG courses; owner has no FireRed / LeafGreen save: "could you make one and test it all on your end?"):
// a blank FireRed / LeafGreen save with a test trainer, N badges, the game-clear (Hall of Fame) flag 0x82C if hof,
// and in box 1 one legal wild Pokemon of each species given (PKHeX's own encounter, the player as OT).
// v0.97: also Ruby / Sapphire / Emerald (RSE courses). Flags (pokeruby / pokeemerald / pokefirered): badges at
// SYSTEM_FLAGS + 7 (RS 0x807, E 0x867) / FLAG_BADGE01 0x820 (FRLG); game clear SYSTEM_FLAGS + 4 (RS 0x804, E 0x864) /
// 0x82C (FRLG).
static (int badge0, int clear) Gen3Flags(GameVersion v) =>
    v is GameVersion.FR or GameVersion.LG ? (0x820, 0x82C) : v == GameVersion.E ? (0x867, 0x864) : (0x807, 0x804);

static SAV3 Blank3(string version, int badges, bool hof, IEnumerable<ushort> species)
{
    var ver = version switch { "LG" => GameVersion.LG, "R" => GameVersion.R, "S" => GameVersion.S, "E" => GameVersion.E,
                               _ => GameVersion.FR };
    var sav = (SAV3)BlankSaveFile.Get(ver, "TESTER", LanguageID.English);
    sav.Version = ver;
    sav.OT = "TESTER";
    sav.TID16 = 24680;
    sav.SID16 = 13579;
    sav.Gender = 1;
    sav.Language = (int)LanguageID.English;
    var (badge0, clear) = Gen3Flags(ver);
    for (int i = 0; i < 8; i++)
        sav.SetEventFlag(badge0 + i, i < badges);
    sav.SetEventFlag(clear, hof);
    if (ver is GameVersion.FR or GameVersion.LG)
        BitConverter.TryWriteBytes(sav.Small[0xAC..], 1u); // FRLG game code (every real FRLG save; PKHeX and PKSM-Core read it)
    else if (ver == GameVersion.E)
    {
        // a real Emerald save has a nonzero security key at 0xAC (PKHeX and PKSM-Core tell Emerald from Ruby / Sapphire
        // by it) and Battle Frontier data at 0xEE0 (PKSM-Core). The key XORs the bag counts and money - the tests use
        // the PC item storage, which has no key.
        BitConverter.TryWriteBytes(sav.Small[0xAC..], 0x5A5A1234u);
        sav.Small[0xEE1] = 0xFF;
    }
    // Ruby / Sapphire: 0 at 0xAC (no Battle Tower record data), as in a real save
    int slot = 0;
    foreach (var sp in species)
    {
        var blank = new PK3 { Species = sp, CurrentLevel = 50 };
        var enc = EncounterMovesetGenerator.GenerateEncounters(blank, ReadOnlyMemory<ushort>.Empty, sav.Version)
                  .Where(e => e.Context == EntityContext.Gen3 && e is not EncounterTrade3)
                  .FirstOrDefault(e => !e.IsEgg);
        if (enc is not IEncounterConvertible conv) { Console.WriteLine("no encounter for " + sp); continue; }
        if (conv.ConvertToPKM(sav) is not PK3 pk) { Console.WriteLine("not a PK3 for " + sp); continue; }
        sav.SetBoxSlotAtIndex(pk, slot++);
    }
    return sav;
}

// A Gen 3 flash image (128 KiB) of a save's three parts, the way the game writes it: slot 1 = 14 sectors of 4 KiB
// (id 0 = trainer block, 1-4 = the large block, 5-13 = PC storage), each with its data (up to 0xF80 bytes), then
// the sector id (u16 at 0xFF4), the checksum (u16 at 0xFF6: the u32 sum of the data, both halves added), the
// signature 0x08012025 (0xFF8) and the save counter (0xFFC). Slot 2 and the rest stay erased flash (0xFF).
static byte[] Flash3(SAV3 sav)
{
    var img = new byte[0x20000];
    Array.Fill(img, (byte)0xFF);
    const int used = 0xF80;
    var parts = new List<byte[]> { sav.Small.ToArray() };
    var large = sav.Large.ToArray();
    for (int o = 0; o < large.Length; o += used)
        parts.Add(large[o..Math.Min(large.Length, o + used)]);
    var storage = sav.Storage.ToArray();
    for (int o = 0; o < storage.Length; o += used)
        parts.Add(storage[o..Math.Min(storage.Length, o + used)]);
    if (parts.Count != 14)
        throw new InvalidOperationException("expected 14 sectors, got " + parts.Count);
    for (int id = 0; id < 14; id++)
    {
        var sector = img.AsSpan(id * 0x1000, 0x1000);
        sector.Clear();
        parts[id].CopyTo(sector);
        uint sum = 0;
        for (int i = 0; i + 4 <= parts[id].Length; i += 4)
            sum += BitConverter.ToUInt32(parts[id], i);
        ushort chk = (ushort)((sum >> 16) + (sum & 0xFFFF));
        BitConverter.TryWriteBytes(sector[0xFF4..], (ushort)id);
        BitConverter.TryWriteBytes(sector[0xFF6..], chk);
        BitConverter.TryWriteBytes(sector[0xFF8..], 0x08012025u);
        BitConverter.TryWriteBytes(sector[0xFFC..], 2u); // slot A holds even counters (the game writes slot counter % 2)
    }
    return img;
}

int bad = 0;
if (args.Length >= 3 && args[0] == "blank3") // pkhex-check blank3 <out> FR|LG [badges] [hof 0/1] [species...]
{
    var sav = Blank3(args[2], args.Length > 3 ? int.Parse(args[3]) : 0, args.Length > 4 && args[4] == "1",
                     args.Skip(5).Select(ushort.Parse));
    var outData = Flash3(sav); // PKHeX cannot Write() its own blank Gen 3 save: the sectors are laid out here
    File.WriteAllBytes(args[1], outData);
    Console.WriteLine($"blocks: small 0x{sav.Small.Length:X} large 0x{sav.Large.Length:X} storage 0x{sav.Storage.Length:X}");
    var clearFlag = Gen3Flags(sav.Version).clear;
    var back = SaveUtil.GetSaveFile(outData) as SAV3;
    if (back is not null && !back.ChecksumsValid) Console.WriteLine("checksums: " + back.ChecksumInfo);
    Console.WriteLine(back is null ? "READ BACK FAILED" : $"read back: {back.GetType().Name} {back.Version} {back.OT} badges {back.Badges} hof {back.GetEventFlag(clearFlag)} sums {back.ChecksumsValid} box {back.BoxData.Count(p => p.Species != 0)}");
    Console.WriteLine($"wrote {args[1]}: {sav.Version} {sav.OT} badges {sav.Badges} hof {sav.GetEventFlag(clearFlag)}");
    foreach (var pk in sav.BoxData.Where(p => p.Species != 0))
        bad += Check(pk, "box", false);
    return bad > 0 ? 1 : 0;
}
if (args.Length >= 2 && args[0] == "memecheck") // Gen 7: is the save's signature valid, and what does it hold?
{
    foreach (var file in args.Skip(1))
    {
        var raw = File.ReadAllBytes(file);
        int off = raw.Length - 0x200 - 0x100; // the signature 0x100 before the block table
        var sig = raw.AsSpan(off, 0x80);
        bool ok = MemeCrypto.VerifyMemeData(sig, out var plain);
        // what it should hold: SHA-256 of the block table (PKHeX signs that)
        var table = raw.AsSpan(raw.Length - 0x200, 0x200);
        var hash = System.Security.Cryptography.SHA256.HashData(table);
        Console.WriteLine($"{Path.GetFileName(file)}: sig at 0x{off:X} valid={ok}; signed bytes {Convert.ToHexString(plain ?? Array.Empty<byte>()).ToLowerInvariant()} ({plain?.Length ?? 0} bytes)");
        Console.WriteLine($"    SHA-256(table) {Convert.ToHexString(hash).ToLowerInvariant()}");
        Console.WriteLine($"    MemeCrypto.SaveFileSignatureOffset 0x{MemeCrypto.SaveFileSignatureOffset:X} length 0x{MemeCrypto.SaveFileSignatureLength:X}");
    }
    return 0;
}
if (args.Length >= 2 && args[0] == "resign") // does PKHeX's own checksum + signature writing change this save?
{
    var raw = File.ReadAllBytes(args[1]);
    var sav = SaveUtil.GetSaveFile(raw);
    if (sav is null) { Console.WriteLine("not a save"); return 2; }
    var again = sav.Write().ToArray();
    int n = 0, first = -1;
    for (int i = 0; i < Math.Min(raw.Length, again.Length); i++)
        if (raw[i] != again[i]) { n++; if (first < 0) first = i; }
    Console.WriteLine($"{sav.Version}: checksums {(sav.ChecksumsValid ? "valid" : "INVALID")}; PKHeX re-signing changes {n} byte(s)" +
                      (n > 0 ? $" (first at 0x{first:X})" : ""));
    return sav.ChecksumsValid && n == 0 ? 0 : 1;
}
if (args.Length >= 3 && args[0] == "gen7diff") // which save blocks / box slots differ between two Gen 7 saves
    return Gen7Check.Diff(args[1], args[2]);
if (args.Length >= 3 && args[0] == "slots") // which box slots differ between two saves (any generation)
{
    var a = SaveUtil.GetSaveFile(args[1]);
    var b = SaveUtil.GetSaveFile(args[2]);
    if (a is null || b is null) { Console.WriteLine("not saves"); return 2; }
    Console.WriteLine($"{a.Version}: checksums before {a.ChecksumsValid}, after {b.ChecksumsValid}; {a.SlotCount} slots");
    int per = a.BoxSlotCount, n = 0;
    for (int i = 0; i < a.SlotCount; i++)
    {
        var pa = a.GetBoxSlotAtIndex(i);
        var pb = b.GetBoxSlotAtIndex(i);
        if (pa.Species != pb.Species || pa.PID != pb.PID || pa.EncryptionConstant != pb.EncryptionConstant)
        {
            n++;
            Console.WriteLine($"box {i / per + 1} slot {i % per + 1}: {GameInfo.Strings.Species[pa.Species]} (PID {pa.PID:X8}) -> " +
                              $"{GameInfo.Strings.Species[pb.Species]} (PID {pb.PID:X8})");
        }
    }
    Console.WriteLine($"{n} slot(s) differ");
    return 0;
}
if (args.Length >= 1 && args[0] == "gen4probe") // where Gen 4 keeps the Hall of Fame time; values in given saves
    return Gen7Check.Probe4(args[1..]);
if (args.Length >= 1 && args[0] == "gen7probe") // where the Hall of Fame sits in Gen 7 saves, trial Z-Crystal ids
    return Gen7Check.Probe();
if (args.Length >= 3 && args[0] == "gen7") // the Gen 7 course sheet against PKHeX (tools/pkhex_check_Gen7Check.cs)
    return Gen7Check.Run(args[1], args[2]);
if (args.Length >= 3 && args[0] == "blank7") // a blank Sun / Moon / Ultra save with a test trainer (PC tests only)
{
    SAV7 sav = Blank7(args[2]);
    if (args.Length >= 4) // v0.64: another game language (LanguageID number) for the language tests
        sav.Language = int.Parse(args[3]);
    File.WriteAllBytes(args[1], sav.Write().ToArray());
    Console.WriteLine($"wrote {args[1]}: {sav.Version} {sav.OT}");
    return 0;
}
if (args.Length >= 2 && args[0] == "hof6") // v0.93: where a Gen 6 save keeps its Hall of Fame (ORAS courses), badges
{
    foreach (var f in args.Skip(1))
    {
        if (SaveUtil.GetSaveFile(f) is not SAV6 sav) { Console.WriteLine("not a Gen 6 save: " + f); bad++; continue; }
        var blocks = sav.GetType().GetProperty("Blocks")!.GetValue(sav)!;
        var hof = (HallOfFame6)blocks.GetType().GetProperty("HallOfFame")!.GetValue(blocks)!;
        ReadOnlySpan<byte> all = sav.Data;
        var e0 = hof.GetEntry(0);
        long off = System.Runtime.CompilerServices.Unsafe.ByteOffset(
            ref System.Runtime.InteropServices.MemoryMarshal.GetReference(all),
            ref System.Runtime.InteropServices.MemoryMarshal.GetReference(e0));
        int idx = HallOfFame6.PokeSize * HallOfFame6.PokeCount; // entry 0's u32 index (HallFame6Index) follows its Pokemon
        var hd = new HallFame6Index(hof.GetEntry(0)[idx..]);
        Console.WriteLine($"{Path.GetFileName(f)} {sav.Version}: HoF entry 0 at 0x{off:X}, index at 0x{off + idx:X} " +
                          $"(has-data bit = byte 0x{off + idx + 3:X} bit 7), entry size 0x{HallOfFame6.EntrySize:X}, " +
                          $"HasData {hd.HasData}, badges {sav.Badges}");
    }
    return bad > 0 ? 1 : 0;
}
if (args.Length >= 5 && args[0] == "templates")
    return Templates.Run(args[1], args[2], args[3], args[4..]);
if (args.Length >= 2 && args[0] == "items") // a save after test_items: checksums + the items PKHeX sees
{
    foreach (var f in args.Skip(1))
    {
        var sav = SaveUtil.GetSaveFile(f);
        if (sav is null) { Console.WriteLine("NOT A SAVE " + f); bad++; continue; }
        Console.WriteLine($"{(sav.ChecksumsValid ? "OK     " : "BADSUM ")} {Path.GetFileName(f)} {sav.Version}");
        if (!sav.ChecksumsValid) bad++;
        foreach (var pouch in sav.Inventory.Pouches)
            foreach (var it in pouch.Items)
                if (it.Count > 0 && (it.Index == 17 || it.Index == 229 || it.Index == 112 || it.Index == 18))
                    Console.WriteLine($"    {pouch.Type}: {GameInfo.Strings.GetItemStrings(sav.Context, sav.Version)[it.Index]} x{it.Count}");
    }
    return bad > 0 ? 1 : 0;
}
if (args.Length >= 2 && args[0] == "names") // v0.64: species names as each game language writes them (romfs i18n)
{
    // <out dir>/<lang>/species.txt (Gen 5+ spelling), species4.txt (Gen 3/4: capitals), species2.txt (Gen 1/2).
    // English is left out: the app's English names stay exactly as they were.
    var langs = new (string Folder, int Id)[] { ("jpn", 1), ("fre", 3), ("ita", 4), ("ger", 5), ("spa", 7), ("kor", 8),
        ("chs", 9), ("cht", 10) };
    foreach (var (folder, id) in langs)
    {
        Directory.CreateDirectory(Path.Combine(args[1], folder));
        foreach (var (file, gen, last) in new[] { ("species.txt", (byte)7, 807), ("species4.txt", (byte)4, 493), ("species2.txt", (byte)2, 251) })
        {
            if (id >= 9 && gen < 7)
                continue; // no Chinese games before Gen 7
            if (id == 8 && gen == 2)
                continue; // Korean Gold / Silver are not supported by the app
            var lines = new List<string>();
            for (int s = 0; s <= last; s++)
                lines.Add(s == 0 ? "" : SpeciesName.GetSpeciesNameGeneration((ushort)s, id, gen));
            File.WriteAllText(Path.Combine(args[1], folder, file), string.Join("\n", lines) + "\n", new System.Text.UTF8Encoding(false));
        }
    }
    Console.WriteLine("species name tables written to " + args[1]);
    return 0;
}
if (args.Length >= 4 && args[0] == "relang") // v0.64: a copy of a save with another game language (PC tests only)
{
    var src = SaveUtil.GetSaveFile(args[1]);
    if (src is null) { Console.WriteLine("not a save: " + args[1]); return 2; }
    src.Language = int.Parse(args[3]);
    File.WriteAllBytes(args[2], src.Write().ToArray());
    return 0;
}
if (args.Length >= 3 && args[0] == "egg4") // v0.66: Gen 4 templates of WILD Pokemon remade as hatched eggs where legal
{
    // pkhex-check egg4 <in catch_dppt.bin> <out>  (user: "Could the DP Froslass not be from an egg?")
    // A wild Gen 4 template keeps its PID and IVs in the app (they are tied to the game's random numbers), so it
    // can never be shiny or get its own nature. The D / P Froslass was wild only because the tool's test egg had
    // hatched male. Every other entry stays byte for byte as it is.
    var bin = File.ReadAllBytes(args[1]);
    int at = 6, changed = 0, failed = 0;
    while (at + 8 <= bin.Length)
    {
        int len = bin[at + 6] | bin[at + 7] << 8;
        if (bin[at + 3] == 4 && len == 136 && (bin[at + 4] & 2) == 0)
        {
            var old = new PK4(bin.AsSpan(at + 8, len).ToArray());
            var moves = old.Moves.ToArray();
            var blank = new PK4 { Species = old.Species, CurrentLevel = old.CurrentLevel };
            blank.Gender = blank.GetSaneGender();
            var tr = new SimpleTrainerInfo(old.Version) { OT = old.OriginalTrainerName, TID16 = old.TID16, SID16 = old.SID16,
                Gender = old.OriginalTrainerGender, Language = old.Language };
            PK4? egg = null;
            foreach (var enc in EncounterMovesetGenerator.GenerateEncounters(blank, moves, old.Version).Where(e => e.IsEgg))
            {
                if (enc is not IEncounterConvertible c) continue;
                for (int tries = 0; tries < 200 && egg is null; tries++)
                {
                    var pk = (PK4)c.ConvertToPKM(tr, EncounterCriteria.Unrestricted);
                    int slot = pk.AbilityNumber >> 1;
                    pk.Species = old.Species;
                    pk.RefreshAbility(slot);
                    if (pk.Gender != blank.Gender && blank.PersonalInfo.IsDualGender == false)
                        continue; // a one-gender evolution needs the egg to have hatched as that gender
                    pk.CurrentLevel = old.CurrentLevel;
                    pk.SetMoves(moves);
                    pk.HealPP();
                    pk.ClearNickname();
                    pk.RefreshChecksum();
                    if (new LegalityAnalysis(pk).Valid)
                        egg = pk;
                }
                if (egg is not null) break;
            }
            string who = $"v{bin[at]} course {bin[at + 1]} poke {bin[at + 2]} {GameInfo.Strings.Species[old.Species]} ({old.Version})";
            if (egg is null) { failed++; Console.WriteLine("stays wild (no legal egg): " + who); }
            else
            {
                egg.Data[..len].CopyTo(bin.AsSpan(at + 8, len));
                bin[at + 4] = (byte)(bin[at + 4] | 2); // flag: egg
                bin[at + 5] = 0;                       // no guaranteed perfect IVs
                changed++;
                Console.WriteLine("now a hatched egg: " + who);
            }
        }
        at += 8 + len;
    }
    File.WriteAllBytes(args[2], bin);
    Console.WriteLine($"{changed} template(s) changed, {failed} left wild -> {args[2]}");
    return 0;
}
if (args.Length >= 3 && args[0] == "handler7") // v0.64: catch_sm.bin with other-version catches marked as received in a trade
{
    // pkhex-check handler7 <in catch_sm.bin> <out>: every entry stays byte for byte as it is, except Gen 7 Pokemon whose
    // origin is another Gen 7 game than the player's (version index 0-3 = SN MN US UM): handler = the player
    // ("PLAYER", as the X / Y Ditto has it). The same rule is in Templates.cs for the next full rebuild.
    var bin = File.ReadAllBytes(args[1]);
    var players = new[] { GameVersion.SN, GameVersion.MN, GameVersion.US, GameVersion.UM };
    int at = 6, changed = 0, illegal = 0, total = 0;
    while (at + 8 <= bin.Length)
    {
        int vi = bin[at], len = bin[at + 6] | bin[at + 7] << 8;
        total++;
        if (bin[at + 3] == 7 && len == 232 && vi < 4)
        {
            var pk = new PK7(bin.AsSpan(at + 8, len).ToArray());
            bool gen7Origin = pk.Version is GameVersion.SN or GameVersion.MN or GameVersion.US or GameVersion.UM;
            if (gen7Origin && pk.Version != players[vi] && pk.CurrentHandler == 0)
            {
                pk.HandlingTrainerName = "PLAYER";
                pk.HandlingTrainerGender = 0;
                pk.CurrentHandler = 1;
                pk.RefreshChecksum();
                if (!new LegalityAnalysis(pk).Valid)
                {
                    illegal++;
                    Console.WriteLine($"NOT LEGAL as received in a trade: v{vi} course {bin[at + 1]} poke {bin[at + 2]} {GameInfo.Strings.Species[pk.Species]}");
                }
                pk.Data[..len].CopyTo(bin.AsSpan(at + 8, len));
                changed++;
            }
        }
        at += 8 + len;
    }
    if (illegal == 0)
        File.WriteAllBytes(args[2], bin);
    Console.WriteLine($"{total} entries, {changed} marked as received in a trade, {illegal} not legal that way" + (illegal == 0 ? " -> " + args[2] : " -> NOT written"));
    return illegal == 0 ? 0 : 1;
}
if (args.Length >= 4 && args[0] == "pkctx") // v0.64: like "pk", but judged with a save loaded, as PKHeX's own window does
{
    // pkhex-check pkctx <gen> <save file | SN | MN | US | UM> <files...>   (a version = the blank test save of "blank7")
    int gen = int.Parse(args[1]);
    SaveFile? sav = args[2] is "SN" or "MN" or "US" or "UM"
        ? Blank7(args[2])
        : SaveUtil.GetSaveFile(args[2]);
    if (sav is null) { Console.WriteLine("not a save: " + args[2]); return 2; }
    ParseSettings.InitFromSaveFileData(sav);
    foreach (var f in args.Skip(3))
    {
        var data = File.ReadAllBytes(f);
        PKM? pk = gen switch { 2 => PokeList2.ReadFromSingle(data), 3 => new PK3(data), 4 => new PK4(data), 6 => new PK6(data), 7 => new PK7(data), _ => null };
        if (pk is null) { Console.WriteLine("cannot read " + f); bad++; continue; }
        bad += Check(pk, Path.GetFileName(f), false);
    }
    return bad > 0 ? 1 : 0;
}
if (args.Length >= 2 && args[0] == "api") // list public members of PKHeX.Core types whose name contains args[1]
{
    foreach (var t in typeof(PKM).Assembly.GetExportedTypes().Where(t => t.Name.Contains(args[1])))
    {
        Console.WriteLine("== " + t.FullName);
        foreach (var m in t.GetMembers(System.Reflection.BindingFlags.Public | System.Reflection.BindingFlags.Static |
                                       System.Reflection.BindingFlags.Instance | System.Reflection.BindingFlags.DeclaredOnly))
            Console.WriteLine("   " + m);
    }
    return 0;
}
if (args.Length >= 2 && args[0] == "pidiv4") // v0.97: which RNG pattern PKHeX sees in Gen 4 Pokemon (an egg needs None)
{
    foreach (var f in args.Skip(1))
    {
        var pk = new PK4(File.ReadAllBytes(f));
        var piv = MethodFinder.Analyze(pk);
        Console.WriteLine($"{Path.GetFileName(f)}: PID {pk.PID:X8} IVs {pk.IV_HP}/{pk.IV_ATK}/{pk.IV_DEF}/{pk.IV_SPA}/{pk.IV_SPD}/{pk.IV_SPE} TID {pk.TID16} SID {pk.SID16} type {piv.Type}");
    }
    return 0;
}
if (args.Length >= 2 && args[0] == "sav")
{
    var sav = SaveUtil.GetSaveFile(args[1]);
    if (sav is null) { Console.WriteLine("not a save: " + args[1]); return 2; }
    Console.WriteLine($"{sav.Version} {sav.OT} gen {sav.Generation}");
    int n = 0;
    foreach (var pk in sav.BoxData)
    {
        if (pk.Species == 0) continue;
        bad += Check(pk, $"box slot {n++}", false);
        if (n >= 40) break;
    }
}
else if (args.Length >= 3 && args[0] == "pk")
{
    int gen = int.Parse(args[1]);
    foreach (var f in args.Skip(2))
    {
        var data = File.ReadAllBytes(f);
        PKM? pk = gen switch
        {
            2 => PokeList2.ReadFromSingle(data), // PKSM's Gen 2 layout = a one-Pokemon list (count, species, 0xFF, ...)
            3 => new PK3(data), // v0.94: Gen 3 (FRLG catches)
            4 => new PK4(data),
            6 => new PK6(data),
            7 => new PK7(data),
            _ => null,
        };
        if (pk is null) { Console.WriteLine("cannot read " + f); bad++; continue; }
        bad += Check(pk, Path.GetFileName(f), args.Contains("-v"));
    }
}
else
{
    Console.WriteLine("usage: pkhex-check sav <file> | pk <gen> <files...>");
    return 2;
}
return bad > 0 ? 1 : 0;
