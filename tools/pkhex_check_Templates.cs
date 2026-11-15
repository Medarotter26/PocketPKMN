// Templates.cs - for every course Pokemon of a PocketPKMN route file and every game version, let PKHeX find a legal
// way it could have been obtained in that game (hidden ability first, then wild, then egg, then the rest), build it at
// the course level with the course moves, and keep it only if PKHeX's own legality check passes.
//   pkhex-check templates <routes.txt> <out.bin> <report.md> <versions...>   (versions: GD SI C | X Y)
// out.bin: "PPCT" u16 version-count, then per entry: u8 version index, u8 course, u8 poke (0-5), u8 gen, u8 flags
// (1 = hidden ability, 2 = egg, 4 = traded history), u8 fixed IV count, u16 data length, data (PK6: 232-byte decrypted
// box data; PK2: 32-byte box data). A course Pokemon without a legal origin has no entry (and is listed in the report).
using System.Text;
using PKHeX.Core;

public static class Templates
{
    record Poke(int Course, int Index, string CourseName, ushort Species, byte Level, ushort[] Moves, string Name);

    static List<Poke> ReadRoutes(string path)
    {
        var list = new List<Poke>();
        int course = -1, idx = 0;
        string cname = "";
        foreach (var raw in File.ReadAllLines(path))
        {
            var line = raw.Split(';')[0].Trim();
            var comment = raw.Contains(';') ? raw[(raw.IndexOf(';') + 1)..].Trim() : "";
            if (line.StartsWith("course "))
            {
                var parts = line.Split('|');
                course = int.Parse(parts[0].Split(' ', StringSplitOptions.RemoveEmptyEntries)[1]);
                cname = parts[1].Trim();
                idx = 0;
            }
            else if (line.StartsWith("poke "))
            {
                var f = line.Split(' ', StringSplitOptions.RemoveEmptyEntries);
                ushort sp = ushort.Parse(f[4]);
                byte lv = byte.Parse(f[5]);
                var mv = f.Skip(7).Take(4).Select(ushort.Parse).Where(m => m != 0).ToArray();
                list.Add(new Poke(course, idx++, cname, sp, lv, mv, comment.Split(':')[0]));
            }
        }
        return list;
    }

    static int Gen(GameVersion v) => v is GameVersion.GD or GameVersion.SI or GameVersion.C ? 2 : 6;

    static GameVersion Ver(string s) => s switch
    {
        "GD" => GameVersion.GD, "SI" => GameVersion.SI, "C" => GameVersion.C, "X" => GameVersion.X, "Y" => GameVersion.Y,
        _ => throw new ArgumentException(s),
    };

    // The trainer the template is made for; the app replaces OT name / IDs / region with the user's own.
    static SimpleTrainerInfo Trainer(GameVersion v) => new(v)
    {
        OT = Gen(v) == 2 ? "PLAYER" : "PLAYER",
        TID16 = 12345, SID16 = (ushort)(Gen(v) == 2 ? 0 : 54321), Gender = 0, Language = (int)LanguageID.English,
        Country = 49, Region = 7, ConsoleRegion = 1,
    };

    static SimpleTrainerInfo OtherTrainer(GameVersion v) => new(v)
    {
        OT = "Trader", TID16 = 22222, SID16 = 33333, Gender = 0, Language = (int)LanguageID.English,
        Country = 49, Region = 7, ConsoleRegion = 1,
    };

    // Turn an encounter into the course Pokemon; null if PKHeX does not call it legal.
    static PKM? Build(IEncounterable enc, Poke p, GameVersion v, bool wantHidden, out bool traded, out string why)
    {
        traded = false;
        why = "";
        if (enc is not IEncounterConvertible conv)
        {
            why = "not convertible";
            return null;
        }
        var tr = Trainer(v);
        var crit = EncounterCriteria.Unrestricted with
        {
            Ability = wantHidden ? AbilityPermission.OnlyHidden : AbilityPermission.Any12,
        };
        PKM pk;
        try { pk = conv.ConvertToPKM(tr, crit); }
        catch (Exception e) { why = "convert: " + e.Message; return null; }
        if (wantHidden && pk.AbilityNumber != 4)
        {
            why = "no hidden ability from this encounter";
            return null;
        }
        if (pk.Species != p.Species) // evolve to the course species, keeping the ability slot
        {
            int slot = pk.AbilityNumber >> 1;
            pk.Species = p.Species;
            pk.RefreshAbility(slot);
            pk.Gender = pk.GetSaneGender();
        }
        if (pk.CurrentLevel < p.Level)
            pk.CurrentLevel = p.Level;
        if (pk.IsEgg)
        {
            why = "still an egg";
            return null;
        }
        Span<ushort> mv = stackalloc ushort[4];
        p.Moves.CopyTo(mv);
        pk.SetMoves(mv);
        pk.HealPP();
        pk.ClearNickname();
        var la = new LegalityAnalysis(pk);
        if (pk.Format >= 6)
        {
            Span<ushort> rel = stackalloc ushort[4];
            MoveListSuggest.GetSuggestedRelearnMovesFromEncounter(la, rel, la.EncounterOriginal);
            pk.SetRelearnMoves(rel);
            la = new LegalityAnalysis(pk);
        }
        if (!la.Valid && pk is IHandlerUpdate h) // a trade evolution on the way: a traded-once history
        {
            var other = OtherTrainer(v);
            h.UpdateHandler(other);
            h.UpdateHandler(tr);
            pk.RefreshChecksum();
            var la2 = new LegalityAnalysis(pk);
            if (la2.Valid)
            {
                traded = true;
                return pk;
            }
        }
        if (!la.Valid)
        {
            why = string.Join(" | ", la.Report().Split('\n').Select(s => s.Trim()).Where(s => s.Length > 0 &&
                !s.StartsWith("Valid", StringComparison.OrdinalIgnoreCase)).Take(3));
            return null;
        }
        pk.RefreshChecksum();
        return pk;
    }

    static int Rank(IEncounterable e, byte level)
    {
        // hidden-ability-capable first, then wild slots, then eggs, then statics/trades/gifts
        int r = e is EncounterSlot6XY s && s.IsHorde ? 0 : 0;
        if (e.IsEgg) return 2;
        if (e.GetType().Name.StartsWith("EncounterSlot")) return 1;
        return 3;
    }

    public static int Run(string routes, string outBin, string report, string[] versions)
    {
        var pokes = ReadRoutes(routes);
        var md = new StringBuilder();
        md.AppendLine($"# Legal catch templates for {Path.GetFileName(routes)}");
        md.AppendLine();
        md.AppendLine("Generated by tools/pkhex_check (PKHeX.Core " +
                      typeof(PKM).Assembly.GetName().Version + "): every row passed PKHeX's legality check.");
        md.AppendLine();
        md.AppendLine("| Course | Pokemon | Lv | " + string.Join(" | ", versions) + " |");
        md.AppendLine("|---|---|---|" + string.Concat(versions.Select(_ => "---|")));
        using var ms = new MemoryStream();
        using var bw = new BinaryWriter(ms);
        bw.Write(Encoding.ASCII.GetBytes("PPCT"));
        bw.Write((ushort)versions.Length);
        int missing = 0, total = 0;
        foreach (var p in pokes)
        {
            var cells = new List<string>();
            for (int vi = 0; vi < versions.Length; vi++)
            {
                var v = Ver(versions[vi]);
                total++;
                PKM blank = Gen(v) == 2 ? new PK2() : new PK6();
                blank.Species = p.Species;
                blank.Form = 0;
                blank.CurrentLevel = p.Level;
                IEnumerable<IEncounterable> encs;
                try { encs = EncounterMovesetGenerator.GenerateEncounters(blank, p.Moves, v).ToList(); }
                catch (Exception e) { cells.Add("ERROR " + e.Message); missing++; continue; }
                var usable = encs.Where(e => e.IsEgg || e.LevelMin <= p.Level).OrderBy(e => Rank(e, p.Level)).ToList();
                PKM? best = null;
                IEncounterable? from = null;
                bool hidden = false, traded = false;
                string lastWhy = usable.Count == 0 ? "no encounter at or below Lv " + p.Level : "";
                if (Gen(v) >= 6) // hidden ability first (user: "ideally, I'd like catches to have hidden abilities")
                    foreach (var e in usable)
                    {
                        best = Build(e, p, v, true, out traded, out lastWhy);
                        if (best != null) { from = e; hidden = true; break; }
                    }
                if (best == null)
                    foreach (var e in usable)
                    {
                        best = Build(e, p, v, false, out traded, out lastWhy);
                        if (best != null) { from = e; break; }
                    }
                if (best == null || from == null)
                {
                    missing++;
                    cells.Add("**NO LEGAL ORIGIN** (" + lastWhy.Replace("|", "/") + ")");
                    continue;
                }
                int fixedIVs = from is IFixedIVSet { IVs.IsSpecified: true } ? 6 :
                               from is IFlawlessIVCount f ? f.FlawlessIVCount : 0;
                byte flags = (byte)((hidden ? 1 : 0) | (from.IsEgg ? 2 : 0) | (traded ? 4 : 0));
                byte[] data = best is PK2 pk2 ? pk2.Data.ToArray()[..32] : best.Data.ToArray()[..232];
                bw.Write((byte)vi); bw.Write((byte)p.Course); bw.Write((byte)p.Index); bw.Write((byte)best.Format);
                bw.Write(flags); bw.Write((byte)fixedIVs); bw.Write((ushort)data.Length); bw.Write(data);
                string loc = from.IsEgg ? "egg" : GameInfo.GetLocationName(false, best.MetLocation, best.Format, best.Format, v);
                cells.Add($"{from.LongName.Replace("|", "/")} - met Lv {best.MetLevel} at {loc}" +
                          (hidden ? " - **hidden ability**" : "") + (traded ? " - traded once" : ""));
            }
            md.AppendLine($"| {p.Course} {p.CourseName} | {p.Name} | {p.Level} | " + string.Join(" | ", cells) + " |");
        }
        File.WriteAllBytes(outBin, ms.ToArray());
        md.Insert(0, "");
        md.AppendLine();
        md.AppendLine($"{total - missing} of {total} have a legal origin; {missing} do not.");
        File.WriteAllText(report, md.ToString());
        Console.WriteLine($"{total - missing}/{total} legal, {missing} without a legal origin -> {report}");
        return missing > 0 ? 1 : 0;
    }
}
