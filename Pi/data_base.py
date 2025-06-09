#!/usr/bin/env python3

import sqlite3

DB_NAME = "noiseDetection.db"
TABLE_NAME = "noises"

def init():
    connection = sqlite3.connect(DB_NAME)               # connect to or create db if not exists 
    cursor = connection.cursor()
    cursor.execute(f"""
    CREATE TABLE IF NOT EXISTS {TABLE_NAME} (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        noise_volume INTEGER,
        created_at DATETIME DEFAULT CURRENT_TIMESTAMP
    )
    """)
    connection.commit()
    connection.close()

def add_noise_reading(noise_volume):
    connection = sqlite3.connect(DB_NAME)        
    cursor = connection.cursor()
    cursor.execute(f"INSERT INTO {TABLE_NAME} (noise_volume) VALUES (?)", (noise_volume,))
    connection.commit()
    connection.close()

def get_all_readings():
    connection = sqlite3.connect(DB_NAME)        
    cursor = connection.cursor()
    cursor.execute(f"SELECT * FROM {TABLE_NAME}")
    for row in cursor.fetchall():
        print(row)
    connection.close()

