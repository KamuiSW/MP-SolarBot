
import tensorflow as tf
try:
    interpreter = tf.lite.Interpreter(model_path="Software/src/Training/PWS_SolarML_Detection/models/encoder.tflite")
    interpreter.allocate_tensors()
    input_details = interpreter.get_input_details()
    output_details = interpreter.get_output_details()
    print("Input Details:", input_details)
    print("Output Details:", output_details)
except Exception as e:
    print("Error loading model:", e)
