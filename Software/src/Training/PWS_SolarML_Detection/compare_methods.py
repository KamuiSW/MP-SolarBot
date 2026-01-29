import time
import numpy as np
import torch
import torchvision.models as models
import torchvision.transforms as transforms
from PIL import Image


# Load and preprocess image

def load_image(path, size=(256, 256)):
    img = Image.open(path).convert("RGB")
    img = img.resize(size)
    return np.array(img, dtype=np.float32)


# Pixel-per-pixel comparison

def pixel_per_pixel(img1, img2):
    # Absolute difference over all RGB pixels
    return np.sum(np.abs(img1 - img2))


# Embedding-based comparison

class EmbeddingModel(torch.nn.Module):
    def __init__(self):
        super().__init__()
        model = models.resnet18(weights=models.ResNet18_Weights.DEFAULT)
        self.feature_extractor = torch.nn.Sequential(*list(model.children())[:-1])

    def forward(self, x):
        x = self.feature_extractor(x)
        return x.view(x.size(0), -1)

def get_embedding(model, img_tensor):
    with torch.no_grad():
        return model(img_tensor)

def cosine_similarity(a, b):
    return torch.nn.functional.cosine_similarity(a, b)


# Main benchmark

def main():

    img1 = load_image("MLReferences/Dirty/Snow-Covered/Snow (40).jpg")
    img2 = load_image("MLReferences/Dirty/Snow-Covered/Snow (39).JPG")

    # Pixel comparison timing
    start = time.perf_counter()
    pixel_result = pixel_per_pixel(img1, img2)
    pixel_time = (time.perf_counter() - start) * 1000

    # Prepare embeddings
    transform = transforms.Compose([
        transforms.ToTensor(),
        transforms.Normalize(
            mean=[0.485, 0.456, 0.406],
            std=[0.229, 0.224, 0.225]
        )
    ])

    img1_t = transform(Image.fromarray(img1.astype(np.uint8))).unsqueeze(0)
    img2_t = transform(Image.fromarray(img2.astype(np.uint8))).unsqueeze(0)

    model = EmbeddingModel()
    model.eval()

    # Embedding comparison timing
    start = time.perf_counter()
    emb1 = get_embedding(model, img1_t)
    emb2 = get_embedding(model, img2_t)
    similarity = cosine_similarity(emb1, emb2)
    embed_time = (time.perf_counter() - start) * 1000

    print("Pixel-per-pixel comparison:")
    print(f"  Values compared: {256*256*3}")
    print(f"  Time: {embed_time:.2f} ms\n")

    print("Embedding-based comparison:")
    print(f"  Embedding size: {emb1.shape[1]} dimensions")
    print(f"  Time: {pixel_time:.2f} ms")
    print(f"  Cosine similarity: {similarity.item():.4f}")

if __name__ == "__main__":
    main()
