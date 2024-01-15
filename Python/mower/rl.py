import base64
from io import BytesIO

from PIL import Image
from flask import Flask
from flask_socketio import SocketIO
from flask_celery import make_celery

# need monkey patch for message queue: https://flask-socketio.readthedocs.io/en/latest/deployment.html#using-multiple-workers
import eventlet

eventlet.monkey_patch()

app = Flask(__name__)
app.config['SECRET_KEY'] = 'there is no secret'

app.config.update(
    broker_url='amqp://localhost//',
    result_backend='rpc://'
)

socketio = SocketIO(app, message_queue='amqp://')

app.celery = make_celery(app)

celery = make_celery(app)


@socketio.on('imageJson')
def process_image(payload):
    # print('jsonpayload', payload)
    name1 = payload['name1']
    encoded_image_data_1 = payload['image1']
    name2 = payload['name2']
    encoded_image_data_2 = payload['image2']
    file_name_1 = name1 + '.png'
    file_name_2 = name2 + '.png'

    arr = payload['arr2']
    print(len(arr))
    i = 0
    vals = []
    print(arr[0], arr[int(arr[0])+1])
    # print('start')
    while i < len(arr):
        s = int(arr[i])
        sli = arr[i: s+1]
        vals.append(sli)
        i+=s+1
    # print('end')
    print(len(vals))
    print(len(vals[0]) + len(vals[1]))
    print(len(vals[1]))

    process_image_task.delay(encoded_image_data_1, file_name_1, encoded_image_data_2, file_name_2)


@celery.task(name='tasks.process_image_task')
def process_image_task(encoded_image_data_1, file_name_1, encoded_image_data_2, file_name_2):
    save_image(encoded_image_data_1, file_name_1)
    save_image(encoded_image_data_2, file_name_2)

    left_throttle = 1
    right_throttle = -1
    # emit response to client
    response = {
        'name': file_name_1,
        'leftThrottle': left_throttle,
        'rightThrottle': right_throttle
    }
    socketio.emit('processedImage', response)


def save_image(encoded_image_data, file_name):
    img_data = base64.b64decode(encoded_image_data)
    img = Image.open(BytesIO(img_data))
    img.save('images/' + file_name, "PNG")


if __name__ == "__main__":
    socketio.run(app, port=8000, debug=True, allow_unsafe_werkzeug=True)
