# PROJECT NERO - Configuración Superset con PostgreSQL
import os

# Configuración básica: las credenciales llegan como variables de entorno
# desde docker-compose.yml, que las lee del archivo .env (ver .env.example)
SECRET_KEY = os.environ["SUPERSET_SECRET_KEY"]

# PostgreSQL como base de datos principal
SQLALCHEMY_DATABASE_URI = (
    f"postgresql://{os.environ['POSTGRES_USER']}:{os.environ['POSTGRES_PASSWORD']}"
    f"@postgres:5432/{os.environ.get('POSTGRES_DB', 'superset')}"
)

# Configuración para tiempo real con ESP32
CACHE_CONFIG = {
    'CACHE_TYPE': 'SimpleCache',
    'CACHE_DEFAULT_TIMEOUT': 1,  # 1 segundo para tiempo real
    'CACHE_KEY_PREFIX': 'project_nero_'
}

# Características habilitadas para PROJECT NERO
FEATURE_FLAGS = {
    "DASHBOARD_AUTO_REFRESH": True,
    "ENABLE_TEMPLATE_PROCESSING": True,
    "DASHBOARD_NATIVE_FILTERS": True,
    "DYNAMIC_PLUGINS": True,
    "DASHBOARD_FILTERS_EXPERIMENTAL": True,
    "THUMBNAILS": True,
    "DASHBOARD_VIRTUALIZATION": True
}

# Configuración específica para datos neurales
ROW_LIMIT = 10000  # Más datos para análisis EEG
SUPERSET_WEBSERVER_TIMEOUT = 300
DEFAULT_SQLLAB_LIMIT = 1000

# Configuración de seguridad
WTF_CSRF_ENABLED = True
WTF_CSRF_EXEMPT_LIST = []
WTF_CSRF_TIME_LIMIT = None

# Logging configurado correctamente
import logging
LOG_LEVEL = 'INFO'

# Configuración de logs para PROJECT NERO
LOGGING_CONFIG = {
    'version': 1,
    'disable_existing_loggers': False,
    'formatters': {
        'default': {
            'format': '🧠 PROJECT NERO [%(asctime)s] %(levelname)s: %(message)s',
        },
    },
    'handlers': {
        'console': {
            'class': 'logging.StreamHandler',
            'level': 'INFO',
            'formatter': 'default',
        },
    },
    'root': {
        'level': 'INFO',
        'handlers': ['console'],
    },
}

# Configuración para conectores de datos en tiempo real
SQLALCHEMY_ENGINE_OPTIONS = {
    'pool_pre_ping': True,
    'pool_recycle': 300,
    'connect_args': {
        'connect_timeout': 10,
    }
}

print("🧠 PROJECT NERO - Configuración PostgreSQL cargada correctamente ✅")