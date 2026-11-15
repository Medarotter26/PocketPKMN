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

int bad = 0;
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
if (args.Length >= 1 && args[0] == "gen7probe") // where the Hall of Fame sits in Gen 7 saves, trial Z-Crystal ids
    return Gen7Check.Probe();
if (args.Length >= 3 && args[0] == "gen7") // the Gen 7 course sheet against PKHeX (tools/pkhex_check_Gen7Check.cs)
    return Gen7Check.Run(args[1], args[2]);
if (args.Length >= 3 && args[0] == "blank7") // a blank Sun / Moon / Ultra save with a test trainer (PC tests only)
{
    SAV7 sav = args[2] is "US" or "UM" ? new SAV7USUM() : new SAV7SM();
    sav.Version = Enum.Parse<GameVersion>(args[2]);
    sav.OT = "TESTER";
    sav.TID16 = 24680;
    sav.SID16 = 13579;
    sav.Gender = 1;
    sav.Language = (int)LanguageID.English;
    sav.Country = 49;
    sav.Region = 7;
    sav.ConsoleRegion = 1;
    File.WriteAllBytes(args[1], sav.Write().ToArray());
    Console.WriteLine($"wrote {args[1]}: {sav.Version} {sav.OT}");
    return 0;
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
