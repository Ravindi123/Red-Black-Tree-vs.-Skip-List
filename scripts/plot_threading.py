import argparse
import math
import os

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import matplotlib.ticker as ticker
import pandas as pd


STRUCTURES = [
    ("RBT_SingleThreaded", "o", "#D95F02", "Red-Black Tree (Sequential Baseline)"),
    ("SkipList_FineGrained", "s", "#1B9E77", "Skip List (Fine-Grained)"),
]


def create_subplot_grid(summary, key_sizes, metric_col, err_col, ylabel, title, outpath, thread_ticks):
    n_keys = len(key_sizes)
    cols = 2
    rows = math.ceil(n_keys / cols)

    fig, axes = plt.subplots(rows, cols, figsize=(6 * cols, 4.5 * rows), squeeze=False)
    axes_flat = axes.flatten()

    for idx, key_size in enumerate(key_sizes):
        ax = axes_flat[idx]
        subset = summary[summary["total_keys"] == key_size]

        for structure, marker, color, label in STRUCTURES:
            s = subset[subset["structure"] == structure].sort_values("threads")
            if s.empty:
                continue
            ax.errorbar(
                s["threads"],
                s[metric_col],
                yerr=s[err_col],
                marker=marker,
                color=color,
                capsize=3,
                label=label,
            )

        ax.set_title(f"Total Keys: {key_size:,}")
        ax.set_xlabel("Number of Threads")
        ax.set_ylabel(ylabel)
        ax.set_xscale("log", base=2)
        ax.set_xticks(thread_ticks)
        ax.get_xaxis().set_major_formatter(ticker.ScalarFormatter())
        ax.grid(True, which="both", linestyle="--", alpha=0.4)
        
        if idx == 0:
            ax.legend()

    # Hide any unused subplots if the number of key sizes is odd
    for idx in range(n_keys, len(axes_flat)):
        axes_flat[idx].set_visible(False)

    fig.suptitle(title, fontsize=14, fontweight="bold", y=0.98)
    fig.tight_layout()
    fig.savefig(outpath, dpi=150)
    plt.close(fig)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", default="results/threading_results.csv")
    parser.add_argument("--outdir", default="results/threading_plots")
    args = parser.parse_args()

    df = pd.read_csv(args.input)
    os.makedirs(args.outdir, exist_ok=True)

    summary = (
        df.groupby(["structure", "total_keys", "threads"])[
            ["throughput_ops_per_sec", "elapsed_ms"]
        ]
        .agg(["mean", "std"])
        .reset_index()
    )
    summary.columns = [
        "structure",
        "total_keys",
        "threads",
        "throughput_mean",
        "throughput_std",
        "elapsed_mean",
        "elapsed_std",
    ]

    key_sizes = sorted(summary["total_keys"].unique())
    thread_ticks = sorted(summary["threads"].unique())

    # Generate the Throughput Subplot Grid
    create_subplot_grid(
        summary=summary,
        key_sizes=key_sizes,
        metric_col="throughput_mean",
        err_col="throughput_std",
        ylabel="Insertion Throughput (ops/sec)",
        title="Concurrent Insertion Throughput",
        outpath=os.path.join(args.outdir, "throughput_subplots.png"),
        thread_ticks=thread_ticks,
    )

    # Generate the Elapsed Time Subplot Grid
    create_subplot_grid(
        summary=summary,
        key_sizes=key_sizes,
        metric_col="elapsed_mean",
        err_col="elapsed_std",
        ylabel="Elapsed Time (ms)",
        title="Concurrent Insertion Elapsed Time",
        outpath=os.path.join(args.outdir, "elapsed_subplots.png"),
        thread_ticks=thread_ticks,
    )

    print(f"Wrote threading subplot grids to {args.outdir}")


if __name__ == "__main__":
    main()