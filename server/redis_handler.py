import redis


class RedisConfig:
    def __init__(self, host: str, port: int, channel: str) -> None:
        self.host = host
        self.port = port
        self.channel = channel


class RedisHandler:
    def __init__(self, config: RedisConfig) -> None:
        self.channel = config.channel
        self.redis_client = redis.StrictRedis(
            host=config.host, port=config.port, decode_responses=True
        )
        self.pubsub = self.redis_client.pubsub()
        self.pubsub.subscribe(self.channel)
        print(f"Subscribed to Redis channel: {self.channel}")

    def listen(self):
        for message in self.pubsub.listen():
            if message["type"] == "message":
                yield message["data"]
