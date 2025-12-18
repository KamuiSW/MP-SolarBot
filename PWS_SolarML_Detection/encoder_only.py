from tensorflow.keras.applications import MobileNetV2
from tensorflow.keras import layers, models, backend as K

IMG_SIZE = (224, 224)

def build_encoder(input_shape=IMG_SIZE + (3,)):
    base = MobileNetV2(weights="imagenet", include_top=False, input_shape=input_shape)
    x = base.output
    x = layers.GlobalAveragePooling2D()(x)
    x = layers.Dense(256, activation="relu")(x)

    x = layers.Lambda(lambda v: K.l2_normalize(v, axis=1))(x)

    encoder = models.Model(inputs=base.input, outputs=x, name="encoder")
    return encoder
