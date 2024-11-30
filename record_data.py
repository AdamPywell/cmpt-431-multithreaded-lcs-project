"""
This script parses the output files generated by running the LCS programs and
extracts the execution times. It then records those execution times in a .csv
file.
"""
import re
import os
import pandas as pd
pd.options.mode.copy_on_write = True

OUT_DIR = 'data'

sequence_lengths = [100, 1000, 10000]
n_runs = 8

def record_serial():
  algo = 'serial'
  os.makedirs(f'{OUT_DIR}/{algo}', exist_ok=True)
  in_dir = f"output/{algo}"
  for sequence_length in sequence_lengths:
    out_dir = f'{OUT_DIR}/{algo}/L{sequence_length}'
    os.makedirs(out_dir, exist_ok=True)
    execution_time = 0.0
    for run in range(1, n_runs + 1):
        file_name = f"{algo}-L{sequence_length}-R{run}.out"
        input_path = f"{in_dir}/L{sequence_length}/{file_name}"
        with open(input_path, 'r') as in_file:
          text = in_file.read()
        pattern = r'(?<=Total time taken:)\s*(\d*[\.]?\d*)'
        match = re.search(pattern, text)
        if match is None:
          raise ValueError(f"ERROR: Could not extract execution time for {file_name}")
        execution_time += float(match.group(1))
    
    avg_execution_time = execution_time / float(n_runs)
    df = pd.DataFrame({
      'avg_execution_time': [avg_execution_time]
    })
    print(df)
    out_path = f"{out_dir}/{algo}-L{sequence_length}.csv"
    df.to_csv(out_path, index=False)
  
def record_parallel():
  algo = 'parallel'
  os.makedirs(f'{OUT_DIR}/{algo}', exist_ok=True)
  in_dir = f"output/{algo}"
  thread_counts = [1, 2, 4, 8]
  for sequence_length in sequence_lengths:
    out_dir = f'{OUT_DIR}/{algo}/L{sequence_length}'
    os.makedirs(out_dir, exist_ok=True)
    avg_execution_times: list[float] = []
    for n_threads in thread_counts:
      execution_time = 0.0
      for run in range(1, n_runs + 1):
        file_name = f"{algo}-L{sequence_length}-T{n_threads}-R{run}.out"
        input_path = f"{in_dir}/L{sequence_length}/{file_name}"
        with open(input_path, 'r') as in_file:
          text = in_file.read()
        pattern = r'(?<=Total time taken:)\s*(\d*[\.]?\d*)'
        match = re.search(pattern, text)
        if match is None:
          raise ValueError(f"ERROR: Could not extract execution time for {file_name}")
        execution_time += float(match.group(1))
        
      avg_execution_time = execution_time / float(n_runs)  
      avg_execution_times.append(avg_execution_time)
    df = pd.DataFrame({
      'n_threads': thread_counts,
      'avg_execution_time': avg_execution_times
    })
    print(df)
    out_path = f"{out_dir}/{algo}-L{sequence_length}.csv"
    df.to_csv(out_path, index=False)
        
def record_distributed():
  algo = 'distributed'
  os.makedirs(f'{OUT_DIR}/{algo}', exist_ok=True)
  in_dir = f"output/{algo}"
  process_counts = [1, 2, 4, 8]
  for sequence_length in sequence_lengths:
    out_dir = f'{OUT_DIR}/{algo}/L{sequence_length}'
    os.makedirs(out_dir, exist_ok=True)
    avg_execution_times: list[float] = []
    for n_processes in process_counts:
      execution_time = 0.0
      
      for run in range(1, n_runs + 1):
        file_name = f"{algo}-L{sequence_length}-P{n_processes}-R{run}.out"
        input_path = f"{in_dir}/L{sequence_length}/{file_name}"
        with open(input_path, 'r') as in_file:
          text = in_file.read()
        pattern = r'(?<=Total time taken:)\s*(\d*[\.]?\d*)'
        match = re.search(pattern, text)
        if match is None:
          raise ValueError(f"ERROR: Could not extract execution time for {file_name}")
        execution_time += float(match.group(1))
      
      avg_execution_time = execution_time / float(n_runs)  
      avg_execution_times.append(avg_execution_time)
    
    df = pd.DataFrame({
      'n_processes': process_counts,
      'avg_execution_time': avg_execution_times
    })
    print(df)
    out_path = f"{out_dir}/{algo}-L{sequence_length}.csv"
    df.to_csv(out_path, index=False)
    
  

def main():
  record_serial()
  # record_parallel()
  record_distributed()

if __name__ == '__main__':
  main()
