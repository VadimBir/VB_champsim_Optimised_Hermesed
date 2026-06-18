#!/usr/bin/env python3
"""
ChampSim Results DB — Query Engine
Usage: python3 query.py DB_PATH [command] [options]
   or: python3 query.py DB_PATH   (interactive menu)
"""
import sys, os, math, sqlite3, argparse

SEP = ";"

def geo_mean(vals):
    v = [x for x in vals if x and x > 0]
    if not v: return None
    return math.exp(sum(math.log(x) for x in v) / len(v))

def conn_open(db_path):
    if not os.path.exists(db_path):
        print(f"DB not found: {db_path}", file=sys.stderr); sys.exit(1)
    c = sqlite3.connect(db_path); c.row_factory = sqlite3.Row
    c.execute("PRAGMA journal_mode=WAL"); return c

def build_where(args):
    clauses, params = ["1=1"], []
    for attr, col, like in [
        ('arch','r.arch',False), ('cores','r.num_cpus',False), ('bypass','r.bypass',False),
        ('pf_l1','r.pf_l1',True), ('pf_l2','r.pf_l2',True), ('trace_filter','r.trace',True)]:
        v = getattr(args, attr, None)
        if v:
            if like: clauses.append(f"{col} LIKE ?"); params.append(f"%{v}%")
            else:     clauses.append(f"{col}=?"); params.append(v if not isinstance(v, str) else v)
    return " AND ".join(clauses), params

def cmd_summary(conn, args):
    n = conn.execute("SELECT COUNT(*) FROM runs").fetchone()[0]
    nt = conn.execute("SELECT COUNT(DISTINCT trace) FROM runs").fetchone()[0]
    np = conn.execute("SELECT COUNT(DISTINCT pf_l1||pf_l2||pf_l3) FROM runs").fetchone()[0]
    archs = [r[0] for r in conn.execute("SELECT DISTINCT arch FROM runs")]
    cores = [str(r[0]) for r in conn.execute("SELECT DISTINCT num_cpus FROM runs ORDER BY num_cpus")]
    print(f"Runs: {n}\nTraces: {nt}\nPF configs: {np}\nArchitectures: {', '.join(archs)}\nCore counts: {', '.join(cores)}")

def cmd_ipc(conn, args):
    w, p = build_where(args)
    rows = conn.execute(f"SELECT r.trace, r.pf_l1, r.pf_l2, r.pf_l3, r.bypass, r.avg_ipc, r.arch, r.num_cpus FROM runs r WHERE {w} ORDER BY r.trace", p).fetchall()
    if not rows: print("No data.", file=sys.stderr); return
    traces = sorted(set(r['trace'] for r in rows))
    pf_keys = sorted(set((r['pf_l1'], r['pf_l2'], r['pf_l3'], r['bypass']) for r in rows))
    data = {}
    for r in rows: data.setdefault((r['pf_l1'], r['pf_l2'], r['pf_l3'], r['bypass']), {})[r['trace']] = r['avg_ipc']
    arch, nc = rows[0]['arch'], rows[0]['num_cpus']
    base_key = next((k for k in pf_keys if k[0]=='no' and k[1]=='no'), None)

    def mk_label(pk):
        s = f"L1D:{pk[0]}+L2C:{pk[1]}+LLC:{pk[2]}"
        return s + f"_ByP:{pk[3]}" if pk[3] else s

    hdr = SEP.join(["Arch","Cores","Prefetcher"] + traces + ["GM","Δ%"])
    print(hdr)
    first_gm = None
    for pk in pf_keys:
        rv = [data.get(pk,{}).get(t) for t in traces]
        gm = geo_mean([v for v in rv if v])
        if first_gm is None: first_gm = gm
        delta = ((gm/first_gm)-1)*100 if gm and first_gm else 0
        print(SEP.join([arch, str(nc), mk_label(pk)] + [f"{v:.5f}" if v else "" for v in rv] + [f"{gm:.6f}" if gm else "", f"{delta:.6f}"]))

    if base_key and base_key in data:
        bd = data[base_key]; print(); print(hdr)
        first_sg = None
        for pk in pf_keys:
            spds = []; cells = []
            for t in traces:
                v, bv = data.get(pk,{}).get(t), bd.get(t)
                if v and bv and bv>0: s=v/bv; spds.append(s); cells.append(f"{s:.6f}")
                else: cells.append("")
            gm = geo_mean(spds)
            if first_sg is None: first_sg = gm
            d = ((gm/first_sg)-1)*100 if gm and first_sg else 0
            print(SEP.join([arch, str(nc), mk_label(pk)] + cells + [f"{gm:.6f}" if gm else "", f"{d:.6f}"]))

def cmd_coverage(conn, args):
    w, p = build_where(args)
    for cn in ["L1D","L2C","LLC"]:
        rows = conn.execute(f"SELECT r.trace, r.pf_l1, r.pf_l2, r.pf_l3, r.bypass, cs.total_miss FROM runs r JOIN cache_stats cs ON r.run_id=cs.run_id WHERE cs.cache_name=? AND cs.cpu=0 AND {w}", [cn]+p).fetchall()
        if not rows: continue
        traces = sorted(set(r['trace'] for r in rows))
        pks = sorted(set((r['pf_l1'],r['pf_l2'],r['pf_l3'],r['bypass']) for r in rows))
        md = {}
        for r in rows: md.setdefault((r['pf_l1'],r['pf_l2'],r['pf_l3'],r['bypass']),{})[r['trace']] = r['total_miss']
        bk = next((k for k in pks if k[0]=='no' and k[1]=='no'), None)
        if not bk: continue
        bm = md.get(bk,{})
        print(f"\n=== Coverage {cn} ===")
        print(SEP.join(["Prefetcher"]+traces+["AVG"]))
        for pk in pks:
            if pk == bk: continue
            covs, cells = [], []
            for t in traces:
                b, m = bm.get(t), md.get(pk,{}).get(t)
                if b and m and b>0: c=(b-m)/b; covs.append(c); cells.append(f"{c:.6f}")
                else: cells.append("")
            avg = sum(covs)/len(covs) if covs else 0
            print(SEP.join([f"L1D:{pk[0]}+L2C:{pk[1]}+LLC:{pk[2]}"]+cells+[f"{avg:.6f}"]))

def cmd_metric(conn, args):
    metric, cache = args.metric, getattr(args,'cache',None) or "L2C"
    w, p = build_where(args)
    rows = conn.execute(f"SELECT r.trace, r.pf_l1, r.pf_l2, r.pf_l3, r.bypass, cs.{metric} as val FROM runs r JOIN cache_stats cs ON r.run_id=cs.run_id WHERE cs.cache_name=? AND cs.cpu=0 AND {w}", [cache]+p).fetchall()
    if not rows: print("No data.", file=sys.stderr); return
    traces = sorted(set(r['trace'] for r in rows))
    pks = sorted(set((r['pf_l1'],r['pf_l2'],r['pf_l3'],r['bypass']) for r in rows))
    data = {}
    for r in rows: data.setdefault((r['pf_l1'],r['pf_l2'],r['pf_l3'],r['bypass']),{})[r['trace']] = r['val']
    print(SEP.join(["Prefetcher"]+traces+["GM"]))
    for pk in pks:
        vals = [data.get(pk,{}).get(t) for t in traces]
        gm = geo_mean([v for v in vals if v is not None])
        print(SEP.join([f"L1D:{pk[0]}+L2C:{pk[1]}+LLC:{pk[2]}"]+[f"{v:.6f}" if v is not None else "" for v in vals]+[f"{gm:.6f}" if gm else ""]))

def cmd_gm_select(conn, args):
    target_str, top_n = args.target, getattr(args,'top',40) or 40
    w, p = build_where(args)
    rows = conn.execute(f"SELECT r.trace, r.pf_l1, r.pf_l2, r.pf_l3, r.bypass, r.avg_ipc FROM runs r WHERE {w}", p).fetchall()
    if not rows: print("No data.", file=sys.stderr); return
    traces = sorted(set(r['trace'] for r in rows))
    pks = sorted(set((r['pf_l1'],r['pf_l2'],r['pf_l3'],r['bypass']) for r in rows))
    data = {}
    for r in rows: data.setdefault((r['pf_l1'],r['pf_l2'],r['pf_l3'],r['bypass']),{})[r['trace']] = r['avg_ipc']
    bk = next((k for k in pks if k[0]=='no' and k[1]=='no'), None)
    if not bk: print("No base found.", file=sys.stderr); return
    bd = data[bk]
    tk = next((k for k in pks if target_str.lower() in (k[0]+k[1]+k[2]).lower()), None)
    if not tk:
        print(f"Target '{target_str}' not found. Available:", file=sys.stderr)
        for k in pks: print(f"  L1D:{k[0]}+L2C:{k[1]}+LLC:{k[2]}", file=sys.stderr)
        return
    others = [k for k in pks if k != bk and k != tk]
    td = data[tk]
    def spd(ts, pd): return [pd.get(t,0)/bd[t] for t in ts if bd.get(t) and pd.get(t)]
    active, removed = list(traces), []
    print(f"Start: {len(active)} traces → target {top_n}")
    print(f"Target: L1D:{tk[0]}+L2C:{tk[1]}+LLC:{tk[2]}")
    while len(active) > top_n:
        tgm = geo_mean(spd(active, td))
        bcg = max((geo_mean(spd(active, data[ok])) or 0) for ok in others) if others else 0
        margin = ((tgm/bcg)-1)*100 if tgm and bcg else 0
        best_t, best_d = None, -1e18
        for t in active:
            rem = [x for x in active if x != t]
            ng = geo_mean(spd(rem, td))
            nc = max((geo_mean(spd(rem, data[ok])) or 0) for ok in others) if others else 0
            nm = ((ng/nc)-1)*100 if ng and nc else 0
            d = nm - margin
            if d > best_d: best_d = d; best_t = t
        if best_t:
            active.remove(best_t); removed.append(best_t)
            ngm = geo_mean(spd(active, td))
            print(f"  Remove [{len(removed):3d}]: {best_t:45s} Δ={best_d:+.4f}% GM={ngm:.6f} left={len(active)}")
    fgm = geo_mean(spd(active, td))
    print(f"\n=== Final {len(active)} traces, GM={fgm:.6f} ===")
    for t in sorted(active): print(f"  {t}")
    print(f"\nRemoved ({len(removed)}):")
    for i, t in enumerate(removed): print(f"  {i+1}. {t}")

def cmd_search(conn, args):
    w, p = build_where(args)
    rows = conn.execute(f"SELECT r.run_id, r.arch, r.num_cpus, r.trace, r.bypass, r.pf_l1, r.pf_l2, r.pf_l3, r.avg_ipc FROM runs r WHERE {w} ORDER BY r.trace", p).fetchall()
    print(SEP.join(["run_id","arch","cpus","trace","bypass","pf_l1","pf_l2","pf_l3","avg_ipc"]))
    for r in rows: print(SEP.join(str(r[k]) if r[k] is not None else "" for k in r.keys()))

def cmd_sql(conn, args):
    q = " ".join(args.sql_query)
    try:
        cur = conn.execute(q)
        if cur.description:
            print(SEP.join(d[0] for d in cur.description))
            for row in cur: print(SEP.join(str(v) if v is not None else "" for v in row))
    except Exception as e: print(f"SQL ERROR: {e}", file=sys.stderr)

def cmd_list(conn, args):
    w = args.what
    if w == "traces":
        for r in conn.execute("SELECT DISTINCT trace FROM runs ORDER BY trace"): print(r[0])
    elif w in ("pf","prefetchers"):
        for r in conn.execute("SELECT DISTINCT pf_l1, pf_l2, pf_l3 FROM runs ORDER BY pf_l1, pf_l2"): print(f"L1D:{r[0]}+L2C:{r[1]}+LLC:{r[2]}")
    elif w == "archs":
        for r in conn.execute("SELECT DISTINCT arch FROM runs"): print(r[0])
    elif w == "bypass":
        for r in conn.execute("SELECT DISTINCT bypass FROM runs"): print(r[0])
    elif w == "cores":
        for r in conn.execute("SELECT DISTINCT num_cpus FROM runs ORDER BY num_cpus"): print(r[0])
    elif w == "metrics":
        for m in ['mpki','apc','lpm','c_amat','avg_miss_lat','total_access','total_hit','total_miss','loads','load_hit','load_miss','rfos','rfo_hit','rfo_miss','prefetches','prefetch_hit','prefetch_miss','writebacks','writeback_hit','writeback_miss','pf_requested','pf_issued','pf_useful','pf_useless','pf_late','stall_load_mshr','stall_rfo','stall_pf_mshr','stall_wrbk']:
            print(f"  {m}")
    else: print(f"Unknown: {w}. Use: traces pf archs bypass cores metrics", file=sys.stderr)

# ── Interactive menu ─────────────────────────────────────────────────────
def interactive(conn):
    C, R = "\033[36m", "\033[0m"
    class A:
        arch=None;cores=None;bypass=None;pf_l1=None;pf_l2=None;trace_filter=None
    def ask(pr, df=None):
        v = input(f"{C}{pr}{R} [{df or ''}]: ").strip(); return v if v else df
    def ask_filters():
        a = A()
        v=ask("arch"); a.arch=v if v else None
        v=ask("cores"); a.cores=int(v) if v else None
        v=ask("bypass"); a.bypass=v if v else None
        v=ask("pf_l1 match"); a.pf_l1=v if v else None
        v=ask("pf_l2 match"); a.pf_l2=v if v else None
        v=ask("trace match"); a.trace_filter=v if v else None
        return a
    while True:
        print(f"\n\033[1m{'='*45}\n ChampSim Results DB — Menu\n{'='*45}\033[0m")
        for n,d in [(1,"Summary"),(2,"IPC Matrix"),(3,"Coverage"),(4,"Metric Pivot"),(5,"GM Trace Select"),(6,"Search"),(7,"List"),(8,"Raw SQL"),(9,"Export CSV"),(0,"Exit")]:
            print(f"  \033[32m{n}\033[0m) {d}")
        ch = input(f"\033[32m> \033[0m").strip()
        try:
            if ch=="0": break
            elif ch=="1": cmd_summary(conn, A())
            elif ch=="2": cmd_ipc(conn, ask_filters())
            elif ch=="3": cmd_coverage(conn, ask_filters())
            elif ch=="4":
                a=ask_filters(); a.metric=ask("metric","mpki"); a.cache=ask("cache","L2C"); cmd_metric(conn,a)
            elif ch=="5":
                a=ask_filters(); a.target=ask("target PF match","Zion"); a.top=int(ask("top N","40")); cmd_gm_select(conn,a)
            elif ch=="6": cmd_search(conn, ask_filters())
            elif ch=="7": a=A(); a.what=ask("what","traces"); cmd_list(conn,a)
            elif ch=="8": a=A(); a.sql_query=[input(f"{C}SQL> {R}")]; cmd_sql(conn,a)
            elif ch=="9":
                a=ask_filters(); rt=ask("type","ipc"); of=ask("file","export.csv")
                import io; old=sys.stdout; sys.stdout=b=io.StringIO()
                if rt=="ipc": cmd_ipc(conn,a)
                elif rt=="coverage": cmd_coverage(conn,a)
                elif rt.startswith("metric"): a.metric=ask("metric","mpki"); a.cache=ask("cache","L2C"); cmd_metric(conn,a)
                sys.stdout=old
                with open(of,'w') as f: f.write(b.getvalue())
                print(f"→ {of}")
        except KeyboardInterrupt: print()
        except Exception as e: print(f"Error: {e}", file=sys.stderr)
        input(f"\n{C}[Enter]{R}")

def main():
    if len(sys.argv) < 2: print("Usage: python3 query.py DB_PATH [command] [options]"); return
    db_path = sys.argv[1]; conn = conn_open(db_path)
    if len(sys.argv) == 2: interactive(conn); conn.close(); return

    p = argparse.ArgumentParser(); p.add_argument("db")
    for a in ['arch','bypass','pf_l1','pf_l2','trace_filter']: p.add_argument(f"--{a}", default=None)
    p.add_argument("--cores", type=int, default=None)
    sub = p.add_subparsers(dest="cmd")
    sub.add_parser("summary"); sub.add_parser("ipc"); sub.add_parser("coverage"); sub.add_parser("search")
    mp=sub.add_parser("metric"); mp.add_argument("metric"); mp.add_argument("--cache",default="L2C")
    gp=sub.add_parser("gm-select"); gp.add_argument("--target",required=True); gp.add_argument("--top",type=int,default=40)
    lp=sub.add_parser("list"); lp.add_argument("what")
    sp=sub.add_parser("sql"); sp.add_argument("sql_query",nargs="+")
    args = p.parse_args()
    cmds = {"summary":cmd_summary,"ipc":cmd_ipc,"coverage":cmd_coverage,"metric":cmd_metric,"gm-select":cmd_gm_select,"search":cmd_search,"list":cmd_list,"sql":cmd_sql}
    if args.cmd in cmds: cmds[args.cmd](conn, args)
    else: p.print_help()
    conn.close()

if __name__ == "__main__": main()
