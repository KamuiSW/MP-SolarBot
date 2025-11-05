import cv2
import numpy as np
import tensorflow.lite as tflite  

MODEL_PATH = "stain_detector.tflite"
THRESHOLD = 0.5

interpreter = tflite.Interpreter(model_path=MODEL_PATH)
interpreter.allocate_tensors()
input_details = interpreter.get_input_details()
output_details = interpreter.get_output_details()

input_height = input_details[0]['shape'][1]
input_width = input_details[0]['shape'][2]

cap = cv2.VideoCapture(0)

while True:
    ret, frame = cap.read()

    img = cv2.resize(frame, (input_width, input_height))
    img = np.expand_dims(img / 255.0, axis=0).astype(np.float32)

    interpreter.set_tensor(input_details[0]['index'], img)
    interpreter.invoke()
    output_data = interpreter.get_tensor(output_details[0]['index'])
    confidence = float(output_data[0][0])

    label = "Dirty" if confidence > THRESHOLD else "Clean"
    color = (0, 0, 255) if label == "Dirty" else (0, 255, 0)
    cv2.putText(frame, f"{label} ({confidence:.2f})", (10, 30),
                cv2.FONT_HERSHEY_SIMPLEX, 1, color, 2)
    cv2.imshow("Stain Detection", frame)

    if cv2.waitKey(1) & 0xFF == ord('q'):
        break

cap.release()
cv2.destroyAllWindows()
