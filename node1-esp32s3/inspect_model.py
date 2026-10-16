from ai_edge_litert.interpreter import Interpreter
import numpy as np

interp = Interpreter(model_path=r'D:\32\esp\my_item\esp32_test\anomaly_detection_model.tflite')
interp.allocate_tensors()
inp = interp.get_input_details()[0]
out = interp.get_output_details()[0]
print('输入:', inp['name'], inp['shape'], inp['dtype'])
print('输出:', out['name'], out['shape'], out['dtype'])
# 用随机输入试跑一次
interp.set_tensor(inp['index'], np.random.rand(*inp['shape']).astype(np.float32))
interp.invoke()
print('试跑输出样例:', interp.get_tensor(out['index']))
print('输入量化参数:', inp.get('quantization_params', None), inp.get('quantization', None))
print('输出量化参数:', out.get('quantization_params', None), out.get('quantization', None))
print('OK')
