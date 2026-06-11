import pandas as pd
from matplotlib import pyplot as plt

df = pd.read_csv('data/synthetic_small1.csv')
df = df[df['symbol'] == 'SYM0']
plt.plot(df['timestamp_ns'], (df['bid_px']+df['ask_px'])/2)
plt.xlabel('timestamp_ns'); plt.ylabel('Mid Price')
plt.show()