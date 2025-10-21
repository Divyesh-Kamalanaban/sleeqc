import tensorflow as tf
import numpy as np
import pandas as pd
from sklearn.model_selection import train_test_split
from sklearn.preprocessing import StandardScaler
# load data
df = pd.read_csv("ML-processing\combined.csv")

X = df[["free_heap_kb", "stack_hwm_bytes", "net_activity", "msg_size", "sign_time_ms"]]
y = df["true_algo"]

scaler = StandardScaler()
X_scaled = scaler.fit_transform(X)

X_train, X_test, y_train, y_test = train_test_split(X_scaled, y, test_size=0.2)

model = tf.keras.models.load_model("adaptive_pqc_model.h5")

converter = tf.lite.TFLiteConverter.from_keras_model(model)
converter.optimizations = [tf.lite.Optimize.DEFAULT]
# Optionally add representative dataset for int8 quantization
def rep_data():
    for i in range(100):
        yield [np.random.rand(1, X_train.shape[1]).astype(np.float32)]
converter.representative_dataset = rep_data
tflite_model = converter.convert()

with open("adaptive_pqc_model_lite.tflite", "wb") as f:
    f.write(tflite_model)
