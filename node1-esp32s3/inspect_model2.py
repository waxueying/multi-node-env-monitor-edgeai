from ai_edge_litert.interpreter import Interpreter

interp = Interpreter(model_path=r'D:\32\esp\my_item\esp32_test\anomaly_detection_model.tflite')
interp.allocate_tensors()

# 列出所有张量
print('=== 张量列表 ===')
for i, t in enumerate(interp.get_tensor_details()):
    print(f'[{i}] {t["name"]} shape={t["shape"]} type={t["dtype"]}')

# 算子统计
print('\n=== 模型算子统计 ===')
ops = interp._get_ops_details() if hasattr(interp, '_get_ops_details') else None
print(ops)
