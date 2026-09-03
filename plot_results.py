import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import numpy as np

def generate_plots(csv_file):
    df = pd.read_csv(csv_file)
    sns.set_theme(style="whitegrid")

    operations = ['Insert', 'Search', 'Delete']
    
    # 1. Runtimes vs N
    fig, axes = plt.subplots(1, 3, figsize=(18, 5))
    for i, op in enumerate(operations):
        op_data = df[(df['Operation'] == op) & (df['Dataset'] == 'Uniform')]
        sns.lineplot(data=op_data, x='N', y='Time_ms', hue='Structure', marker='o', ax=axes[i], errorbar='sd')
        axes[i].set_title(f'{op} Time vs N (Uniform)')
        axes[i].set_xscale('log')
        axes[i].set_yscale('log')
    plt.tight_layout()
    plt.savefig("runtime_vs_n.png")

    # 2. Structural Height vs N against theoretical log(n)
    plt.figure(figsize=(8, 6))
    height_data = df[(df['Operation'] == 'Insert') & (df['Dataset'] == 'Uniform')]
    sns.lineplot(data=height_data, x='N', y='Height', hue='Structure', marker='o', errorbar='sd')
    
    # Plot baseline theoretical logs
    n_unique = sorted(df['N'].unique())
    plt.plot(n_unique, [np.log2(n) for n in n_unique], 'k--', label='Theoretical log2(n)')
    
    plt.title('Structural Height vs N (Uniform)')
    plt.xscale('log')
    plt.legend()
    plt.savefig("height_vs_n.png")

    # 3. Memory Footprint vs N
    plt.figure(figsize=(8, 6))
    mem_data = df[(df['Operation'] == 'Insert') & (df['Dataset'] == 'Uniform')]
    sns.lineplot(data=mem_data, x='N', y='Memory_bytes', hue='Structure', marker='o')
    plt.title('Memory Footprint vs N (Uniform)')
    plt.xscale('log')
    plt.yscale('log')
    plt.savefig("memory_vs_n.png")
    
    print("Plots generated successfully: runtime_vs_n.png, height_vs_n.png, memory_vs_n.png")

if __name__ == "__main__":
    generate_plots("benchmark_results.csv")