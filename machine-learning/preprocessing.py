import numpy as np
import pandas as pd

dilithium2_p1 = pd.read_csv('dataset-suite\dilithium2-p1.csv')
dilithium2_p2 = pd.read_csv('dataset-suite\dilithium2-p2.csv')

dilithium2 = pd.concat([dilithium2_p1, dilithium2_p2], ignore_index=True)
dilithium5 = pd.read_csv('dataset-suite\dilithium5.csv')

dilithium2['true_algo'] = 0
dilithium5['true_algo'] = 1

combined_df = pd.concat([dilithium2, dilithium5], ignore_index=True)

combined_df.to_csv('dataset-suite\combined.csv', index=False)