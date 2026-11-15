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
