import tensorflow as tf
from sklearn.model_selection import train_test_split
from sklearn.preprocessing import StandardScaler
import pandas as pd

# load data
df = pd.read_csv("ML-processing\combined.csv")

X = df[["free_heap_kb", "stack_hwm_bytes", "net_activity", "msg_size", "sign_time_ms"]]
y = df["true_algo"]

scaler = StandardScaler()
X_scaled = scaler.fit_transform(X)

X_train, X_test, y_train, y_test = train_test_split(X_scaled, y, test_size=0.2)

#load model
model = tf.keras.models.load_model("adaptive_pqc_model.h5")

print("Model Summary:")
model.summary()


# Evaluate the model on the test data using `evaluate`
print("Evaluate on test data")
results = model.evaluate(X_test, y_test, batch_size=128)
print("test loss, test acc:", results)



X_new = pd.DataFrame({
    "free_heap_kb": 245.57,
    "stack_hwm_bytes": 10268,
    "net_activity": 1,
    "msg_size": 512,
    "sign_time_ms": 150
}, index=[0])

# Generate predictions (probabilities -- the output of the last layer)
# on new data using `predict`
print("Generate predictions for new samples")
predictions = model.predict(X_new)
print("predictions shape:", predictions)