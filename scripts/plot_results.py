import os
import argparse
import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import numpy as np

def generate_plots(csv_file, outdir):
    df = pd.read_csv(csv_file)
    sns.set_theme(style="whitegrid")

    operations = ['Insert', 'Search', 'Delete']
    datasets = ['Uniform', 'Sorted', 'Skewed', 'Real']
    
    # 1. Runtimes vs N
    fig, axes = plt.subplots(4, 3, figsize=(18, 20))
    for j, dataset in enumerate(datasets):
        for i, op in enumerate(operations):
            op_data = df[(df['Operation'] == op) & (df['Dataset'] == dataset)]
            sns.lineplot(data=op_data, x='N', y='Time_ms', hue='Structure', marker='o', ax=axes[j, i], errorbar='sd', estimator='mean')
            axes[j, i].set_title(f'{op} Time vs N ({dataset})')
            axes[j, i].set_xscale('log')
            axes[j, i].set_yscale('log')
    plt.tight_layout()
    plt.savefig(os.path.join(outdir, "runtime_vs_n.png"))

    # 2. Structural Height vs N against theoretical log(n)
    fig, axes = plt.subplots(1,4, figsize=(18, 6))
    for i, dataset in enumerate(datasets):
        data = df[(df['Operation'] == 'Insert') & (df['Dataset'] == dataset)]
        sns.lineplot(data=data, x='N', y='Height', hue='Structure', marker='o', errorbar='sd', estimator='mean', ax=axes[i])
    
    # Plot baseline theoretical logs
    n_unique = sorted(df['N'].unique())
    
    for i, dataset in enumerate(datasets):
        axes[i].plot(n_unique, [np.log2(n) for n in n_unique], 'k--', label='Theoretical log2(n)')
        axes[i].set_title(f'Structural Height vs N ({dataset})')
        axes[i].set_xscale('log')
        axes[i].legend()
    plt.tight_layout()
    plt.savefig(os.path.join(outdir, "height_vs_n.png"))

    # 3. Memory Footprint vs N
    fig, axes = plt.subplots(1,4, figsize=(18, 6))
    for i, dataset in enumerate(datasets):
        data = df[(df['Operation'] == 'Insert') & (df['Dataset'] == dataset)]
        sns.lineplot(data=data, x='N', y='Memory_bytes', hue='Structure', marker='o', errorbar='sd', estimator='mean', ax=axes[i])
        axes[i].set_title(f'Memory Footprint vs N ({dataset})')
        axes[i].set_xscale('log')
        axes[i].set_yscale('log')
    plt.tight_layout()
    plt.savefig(os.path.join(outdir, "memory_vs_n.png"))
    
    print(f"Plots generated successfully: {os.path.join(outdir, 'runtime_vs_n.png')}, {os.path.join(outdir, 'height_vs_n.png')}, {os.path.join(outdir, 'memory_vs_n.png')}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", default="results/benchmark_results_optimized.csv")
    parser.add_argument("--outdir", default="results/optimized_plots")
    args = parser.parse_args()

    df = pd.read_csv(args.input)
    os.makedirs(args.outdir, exist_ok=True)
    generate_plots(args.input, args.outdir)