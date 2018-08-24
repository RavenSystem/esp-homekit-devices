#!/usr/bin/env python

from collections import namedtuple
from flask import Flask, render_template
import os
import os.path


Network = namedtuple('Network', ['ssid', 'secure'])

WIFI_NETWORKS = [
    Network(ssid='Network1', secure=False),
    Network(ssid='Network2', secure=False),
    Network(ssid='Network3', secure=False),
    Network(ssid='Network4', secure=True),
    Network(ssid='Network5', secure=True),
    Network(ssid='Network6', secure=True),
    Network(ssid='MaximumNetworkNameMaximumNetwork', secure=False),
    Network(ssid='MaximumNetworkNameMaximumNetwork', secure=True),
]


base_path = os.getcwd()
app = Flask(
    __name__,
    static_folder=os.path.join(base_path, 'content'),
    template_folder=os.path.join(base_path, 'content'),
)


@app.template_filter('ternary')
def ternary(x, true, false):
    return true if x else false


@app.route('/settings', methods=['GET'])
def get_settings():
    return render_template('index.html', networks=WIFI_NETWORKS)


@app.route('/settings0', methods=['GET'])
def get_settings0():
    return render_template('index.html', networks=[])


@app.route('/settings', methods=['POST'])
def update_settings():
    return 'Connecting to %s, password = %s' % (request.form['ssid'], request.form['password'])


app.run(host='0.0.0.0', debug=True)
