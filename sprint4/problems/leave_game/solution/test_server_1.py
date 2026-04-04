import json
import random
import math
import time
import re
import os

from enum import Enum
from typing import Optional, Tuple, List, Union, Type, KeysView, Any
from dataclasses import dataclass
from collections import defaultdict

from urllib.parse import urljoin

import requests
import pytest


@dataclass
class Cache:
    tokens: List[str] = None
    state: dict = None

    def __post_init__(self):
        self.tokens = list()
        self.state = dict()


class ServerException(Exception):
    def __init__(self, message: str, data: Any):
        super().__init__()
        self.__message = message
        self.__data = data
        self.args = message, data

    def message(self):
        return self.__message

    def data(self):
        return self.__data

    def __str__(self):
        try:
            return f"{self.message}: {json.dumps(self.data, default=str)}"
        except (TypeError, ValueError):
            return f"{self.message}: {repr(self.data)}"


class DataInconsistency(ServerException):
    """
    Given data doesn't have all the expected fields or values are not sufficient
    """


class UnexpectedData(DataInconsistency):
    """
    One of the field is missing
    """

    def __init__(self, parent_object: str, expected: Any, given: Any):
        super().__init__(f"{parent_object} has unexpected data",
                         {'expected': expected, 'given': given})
        self.__parent_object = parent_object
        self.__expected = expected
        self.__given = given
        self.args = parent_object, expected, given

    def parent_object(self):
        return self.__parent_object

    def expected(self):
        return self.__expected

    def given(self):
        return self.__given

    def __str__(self):
        return f"{self.parent_object} has unexpected data:\n{json.dumps(self.expected)} was expected, " \
               f"but {json.dumps(self.given)} was given"


class WrongFields(UnexpectedData):
    """
    One of the field is missing
    """

    def __str__(self):
        return f"{self.parent_object} has wrong fields:\n{json.dumps(self.expected)} was expected, " \
               f"but {json.dumps(self.given)} was given"


class WrongType(DataInconsistency):
    def __init__(self, parent_object: str, expected_type: Union[Type, List[Type]], given_type: Type):

        expected_type = [t.__name__ for t in list(expected_type)]

        super().__init__(f"{parent_object} has wrong fields",
                         {'expected type': expected_type, 'given type': given_type.__name__})

        self.__parent_object = parent_object
        self.__expected_type = expected_type
        self.__given_type = given_type
        self.args = parent_object, expected_type, given_type

    def parent_object(self):
        return self.__parent_object

    def expected_type(self):
        return self.__expected_type

    def given_type(self):
        return self.__given_type

    def __str__(self):
        return f"{self.parent_object} has wrong type:\n{json.dumps(self.expected_type)} was expected, " \
               f"but it's {json.dumps(self.given_type.__name__)}"


class BadRequest(ServerException):
    """
    The request was bad - token is wrong or missing, url isn't valid, etc.
    """


class GameServer:

    def __init__(self,
                 server_domain: str,
                 port: Union[str, int] = '8080',
                 **extra_kwargs):
        self.url = f'http://{server_domain}:{port}'
        self.port = port
        self.container = None  # For Docker container reference

    def __enter__(self, **kwargs):
        self.__init__(**kwargs)

    def request(self, method, header, url, **kwargs):
        try:
            req = requests.Request(method, urljoin(self.url, url), headers=header, **kwargs).prepare()
            with requests.Session() as session:
                return session.send(req)
        except Exception as ex:
            print(f"Request error: {ex}")
            return None

    def get(self, endpoint):
        return requests.get(urljoin(self.url, endpoint))

    def post(self, endpoint, data):
        return requests.post(urljoin(self.url, endpoint), data)

    def get_maps(self) -> Optional[List[dict]]:
        request = 'api/v1/maps'
        res: requests.Response = self.get(request)
        self.validate_response(res)
        res_json: List[dict] = res.json()

        GameServer.assert_type('Map list', list, res_json)

        for m in res_json:
            GameServer.assert_fields('Map', ['id', 'name'], m.keys())
            GameServer.assert_type('Map id', str, m['id'])
            GameServer.assert_type('Map name', str, m['name'])

        return res_json

    def get_map(self, map_id: str) -> Optional[dict]:
        request = 'api/v1/maps/' + map_id
        res: requests.Response = self.get(request)
        self.validate_response(res)
        self.validate_map(res.json())
        return res.json()

    def join(self, player_name: str, map_id: str) -> Tuple[str, int]:
        print(f"DEBUG: Joining game with name='{player_name}', map_id='{map_id}'")
        request = 'api/v1/game/join'
        header = {'content-type': 'application/json'}
        data = {"userName": player_name, "mapId": map_id}
        res = self.request('POST', header, request, json=data)
        
        if res is None:
            raise Exception("Failed to connect to server")
        
        print(f"DEBUG: Join response status: {res.status_code}")
        print(f"DEBUG: Join response body: {res.text}")
        
        res_json: dict = res.json()

        GameServer.assert_fields('Join game response', ['authToken', 'playerId'], res_json.keys())

        token = res_json['authToken']
        self.validate_token(token)

        player_id = res_json['playerId']
        GameServer.assert_type('Player id', int, player_id)

        print(f"DEBUG: Join successful - token={token[:8]}..., player_id={player_id}")
        return token, player_id

    def add_player(self, player_name: str, map_id: str) -> Tuple[str, int]:
        params = self.join(player_name, map_id)
        return params

    def get_state(self, token: str) -> Optional[dict]:
        request = '/api/v1/game/state'
        header = {'content-type': 'application/json',
                  'Authorization': f'Bearer {token}'}

        res = self.request('GET', header, request)
        self.validate_response(res)
        res_json = res.json()
        self.validate_state(res_json)
        return res_json

    def get_player_state(self, token: str, player_id: int) -> Optional[dict]:
        game_session_state = self.get_state(token)

        self.assert_type('Game session state', dict, game_session_state)
        self.assert_fields('Game session state', 'players', game_session_state.keys())

        players = game_session_state.get('players')

        self.assert_type('Players state', dict, players)
        state = players.get(str(player_id))

        if state is None:
            raise DataInconsistency('Game state doesn\'t have the given player id',
                                    {'player_id': players, 'game_state': game_session_state})

        self.validate_player_state(state)

        return state

    def move(self, token: str, direction: str):
        print(f"DEBUG: Move - token={token[:8]}..., direction='{direction}'")
        request = '/api/v1/game/player/action'
        header = {'content-type': 'application/json', 'Authorization': f'Bearer {token}'}
        data = {"move": direction}
        res = self.request('POST', header, request, json=data)
        self.validate_response(res)
        print(f"DEBUG: Move successful")

    def tick(self, ticks: int):
        print(f"DEBUG: Tick - ticks={ticks}ms")
        request = 'api/v1/game/tick'
        header = {'content-type': 'application/json'}
        data = {"timeDelta": ticks}
        res = self.request('POST', header, request, json=data)
        self.validate_response(res)
        print(f"DEBUG: Tick successful")

    def get_records(self, start: int = 0, max_items: int = 100) -> list:
        print(f"DEBUG: Get records - start={start}, max_items={max_items}")
        request = '/api/v1/game/records'
        header = {'content-type': 'application/json'}
        url_params = {'start': start, 'maxItems': max_items}
        res: requests.Response = self.request('GET', header, request, params=url_params)
        self.validate_response(res)
        res_json: list = res.json()
        assert type(res_json) == list
        print(f"DEBUG: Got {len(res_json)} records")
        return res_json

    @staticmethod
    def assert_type(obj_name: str, expected_types: Union[Type, List[Type]], obj: any):
        if type(expected_types) not in {list, tuple, set}:
            expected_types = [expected_types]
        if type(obj) not in expected_types:
            raise WrongType(obj_name, expected_types, type(obj))

    @staticmethod
    def assert_fields(object_name, expected_keys: Union[list, str, KeysView], given_keys: KeysView):
        if isinstance(expected_keys, str):
            expected_keys = [expected_keys]
        else:
            expected_keys = list(expected_keys)

        for key in expected_keys:
            if key not in given_keys:
                raise WrongFields(object_name, list(expected_keys), list(given_keys))

    @staticmethod
    def validate_response(res: requests.Response):
        if res is None:
            raise Exception("Response is None - server may not be running")
            
        print(f"DEBUG: Response status: {res.status_code}, url: {res.url}")
        
        if res.status_code != 200:
            print(f"ERROR: Status code {res.status_code}")
            print(f"ERROR: Response body: {res.text}")
            print(f"ERROR: Request headers: {res.request.headers}")
            raise BadRequest(f'Status code isn\'t OK: {res.status_code}',
                           {'status code': res.status_code,
                            'response': res.text})

        GameServer.assert_fields('Response headers',
                                ['content-type', 'cache-control', 'content-length'],
                                res.headers.keys())

        if res.headers['content-type'] != 'application/json':
            raise UnexpectedData('Content-type', 'application/json', res.headers['content-type'])

        if res.headers['cache-control'] != 'no-cache':
            raise UnexpectedData('Cache-control', 'no-cache', res.headers['cache-control'])

        if res.request.method != 'HEAD':
            if int(res.headers['content-length']) != len(res.content):
                raise UnexpectedData('Headers\' content-length', 
                                     len(res.content), 
                                     int(res.headers['content-length']))
        else:
            if res.headers['content-length'] != 0:
                raise UnexpectedData('Headers\' content-length for head request should be zero',
                                     0, int(res.headers['content-length']))

        try:
            res.json()
        except json.decoder.JSONDecodeError as je:
            raise DataInconsistency('The response has badly encoded JSON',
                                    {'response content': res.content, 
                                     'JSON decoder error': [je.msg, je.doc, je.pos]}) from je

    @staticmethod
    def validate_map(m: dict):
        expected = {'id': str, 'name': str, 'roads': list, 'buildings': list, 'offices': list}

        GameServer.assert_fields('Map', expected.keys(), m.keys())

        for key, type_key in expected.items():
            GameServer.assert_type(key, type_key, m[key])

        extra = {'dogSpeed': float}

        for key, type_key in extra.items():
            if key in m.keys():
                GameServer.assert_type(key, type_key, m[key])

        dog_speed = m.get('dogSpeed')
        if dog_speed is not None:
            if dog_speed < 0:
                raise DataInconsistency('Dog speed can\'t be negative', {'dog speed': dog_speed})

        for road in m['roads']:
            road: dict
            GameServer.assert_type('Road', dict, road)

            if road.keys() != {'x0', 'y0', 'x1'} and road.keys() != {'x0', 'y0', 'y1'}:
                raise WrongFields('Road',
                                  '["x0", "y0", "x1"] or ["x0", "y0", "y1"]', 
                                  list(road.keys()))

            for coordinate in road:
                GameServer.assert_type(f'Road coordinate {coordinate}',
                                      [float, int],
                                      road[coordinate])

        for building in m['buildings']:
            building: dict

            GameServer.assert_type('Building on the map', dict, building)
            GameServer.assert_fields('Building on the map', ['x', 'y', 'w', 'h'], building.keys())

            for field in building:
                GameServer.assert_type(f'Building field {field}', [float, int], building[field])
                if field in ['w', 'h']:
                    if building[field] <= 0:
                        raise DataInconsistency('Building size is\'t positive', {'building': building})

        for office in m['offices']:
            office: dict
            GameServer.assert_type('Office', dict, office)

            expected = {'id': str, 'x': [float, int], 'y': [float, int],
                        'offsetX': [float, int], 'offsetY': [float, int]}

            GameServer.assert_fields('Office on the map', expected.keys(), office.keys())

            for field, type_field in expected.items():
                GameServer.assert_type(f'Office field {field}', type_field, office[field])

    @staticmethod
    def validate_token(token: str):
        try:
            int(token, 16)
        except ValueError as exc:
            raise DataInconsistency('Token is invalid, it should be a hex value', {'token': token}) from exc

    @staticmethod
    def validate_state(res_json: dict):
        GameServer.assert_type('Game state', dict, res_json)
        players = res_json.get('players')
        GameServer.assert_type('Game state, players', dict, players)
        for player_id in players:
            GameServer.assert_type('Player id', [str, int], player_id)
            player: dict = players[player_id]

            GameServer.validate_player_state(player)

    @staticmethod
    def validate_player_state(state: dict):

        GameServer.assert_type('player_id', dict, state)

        expected = {'pos': list, 'speed': list, 'dir': str}
        for field, typeField in expected.items():
            GameServer.assert_fields('Player state', expected.keys(), state.keys())
            GameServer.assert_type(field, typeField, state[field])

        for coordinate in state['pos']:
            GameServer.assert_type('Player position', float, coordinate)
        for coordinate in state['speed']:
            GameServer.assert_type('Player speed', float, coordinate)

        GameServer.assert_type('Player direction', str, state['dir'])
        expected_dirs = ['R', 'L', 'U', 'D', '']
        if state['dir'] not in expected_dirs:
            raise UnexpectedData('Player direction', ['R', 'L', 'U', 'D', ''], state['dir'])


class Direction(Enum):
    U = 1
    R = 2
    D = 3
    L = 4

    def __str__(self):
        return self.name

    @staticmethod
    def random():
        return Direction[Direction.random_str()]

    @staticmethod
    def random_str():
        return random.choice(Direction.__dict__['_member_names_'])


@dataclass
class Player:

    name: str
    token: str
    player_id: int
    score: float = 0
    playing_time: float = 0.0

    def add_time(self, time_to_add: float):
        self.playing_time += time_to_add

    def get_dict(self) -> dict:
        return {
            "name": self.name,
            "score": self.score,
            "playTime": self.playing_time
        }

    def update_score(self, server):
        state = server.get_player_state(self.token, self.player_id)
        self.score = state['score']


class Tribe:

    def __init__(self,
                 server: GameServer,
                 map_id: str,
                 r_time: float = 15.0,
                 num_of_players: int = 10,
                 prefix: str = 'Player'):
        self.server = server
        self.players: List[Player] = []
        self.r_time = r_time
        print(f"DEBUG: Creating tribe with {num_of_players} players on map '{map_id}'")
        for i in range(0, num_of_players):
            name = f'{prefix} {i}'
            print(f"DEBUG: Adding player {i+1}/{num_of_players}: {name}")
            token, player_id = server.join(name, map_id)
            self.players.append(Player(name, token, player_id))
        print(f"DEBUG: Tribe created successfully")

    def __getitem__(self, index: int) -> Player:
        return self.players[index]

    def __tick_seconds(self, seconds: float):
        self.server.tick(int(seconds*1_000))

    def add_time(self, time_to_add: float):
        for pl in self.players:
            pl.add_time(time_to_add)

    def get_list(self) -> list:
        self.players.sort(key=lambda x: x.score, reverse=True)
        res = [pl.get_dict() for pl in self.players]
        return res

    def update_scores(self):
        print(f"DEBUG: Updating scores for {len(self.players)} players")
        for player in self.players:
            player.update_score(self.server)
        print(f"DEBUG: Scores updated")

    def randomized_turn(self):
        for pl in self.players:
            direction = Direction.random_str()
            self.server.move(pl.token, direction)

    def randomized_move(self):
        print(f"DEBUG: Randomized move")
        self.randomized_turn()
        ticks = random.randint(100, min(10000, int(self.r_time*900)))
        seconds = ticks / 1000
        self.add_time(seconds)
        self.__tick_seconds(seconds)

    def stop(self):
        print(f"DEBUG: Stopping all players")
        for pl in self.players:
            self.server.move(pl.token, '')


def compare(records: List[dict], tribe_records: List[dict]):
    print(f"DEBUG: Comparing records - records count: {len(records)}, tribe_records count: {len(tribe_records)}")
    assert len(records) == len(tribe_records)
    for record in records:
        name = record['name']
        for t_record in tribe_records:
            if t_record['name'] == name:
                math.isclose(record['score'], t_record['score'])


def add_user_and_wait_loot(docker_server, name, map_id):
    token, _ = docker_server.join(name, map_id)
    docker_server.tick(5000*1000)
    docker_server.tick(5000*1000)
    docker_server.tick(5000*1000)
    docker_server.tick(5000*1000)
    return token


def tick_seconds(server, seconds: float):
    """Helper function to tick for a given number of seconds"""
    server.tick(int(seconds * 1000))


def get_retirement_time() -> float:
    """Get retirement time from config or return default"""
    DEFAULT_RETIREMENT_TIME = 60.0
    try:
        # Try to read from config file
        config_path = os.path.join(os.path.dirname(__file__), 'data', 'config.json')
        if os.path.exists(config_path):
            with open(config_path, 'r') as f:
                config = json.load(f)
                return config.get('dogRetirementTime', DEFAULT_RETIREMENT_TIME)
    except Exception:
        pass
    return DEFAULT_RETIREMENT_TIME


# ==================== TESTS ====================

def test_clean_records():
    """Test that records are empty initially"""
    print("\n=== test_clean_records ===")
    server = GameServer("127.0.0.1", 8080)
    records = server.get_records()
    assert len(records) == 0
    print("✓ test_clean_records passed")


def test_retirement_one_standing_player():
    """Test that a standing player retires after timeout"""
    print("\n=== test_retirement_one_standing_player ===")
    server = GameServer("127.0.0.1", 8080)
    r_time = get_retirement_time()
    
    token, player_id = server.join('Julius Can Standing', "map1")
    server.get_state(token)
    
    # Tick almost up to retirement time
    tick_seconds(server, r_time - 0.001)
    server.get_state(token)  # Should still be active
    
    # Tick the last millisecond
    tick_seconds(server, 0.001)
    
    # Try to get state - should return 401
    request = '/api/v1/game/state'
    header = {'content-type': 'application/json', 'Authorization': f'Bearer {token}'}
    res = server.request('GET', header, request)
    
    assert res.status_code == 401
    
    records = server.get_records()
    assert len(records) >= 1
    assert records[0]['name'] == 'Julius Can Standing'
    assert records[0]['score'] == 0
    assert math.isclose(records[0]['playTime'], r_time, rel_tol=0.1)
    print("✓ test_retirement_one_standing_player passed")


def test_retirement_one_player():
    """Test that a moving player retires after stopping"""
    print("\n=== test_retirement_one_player ===")
    server = GameServer("127.0.0.1", 8080)
    r_time = get_retirement_time()
    
    token, player_id = server.join('Julius Can Moving', "map1")
    server.get_state(token)
    
    # Move around for a while
    for _ in range(100):
        direction = Direction.random_str()
        server.move(token, direction)
        server.tick(random.randint(10, int(r_time * 900)))
    
    state = server.get_player_state(token, player_id)
    score = state['score']
    
    # Stop and wait for retirement
    server.move(token, '')
    tick_seconds(server, r_time)
    
    # Verify token is invalid
    request = '/api/v1/game/state'
    header = {'content-type': 'application/json', 'Authorization': f'Bearer {token}'}
    res = server.request('GET', header, request)
    assert res.status_code == 401
    
    records = server.get_records()
    assert records[0]['name'] == 'Julius Can Moving'
    assert math.isclose(float(records[0]['score']), score, rel_tol=0.1)
    print("✓ test_retirement_one_player passed")


def test_a_few_zero_records():
    """Test with zero-score players"""
    print("\n=== test_a_few_zero_records ===")
    server = GameServer("127.0.0.1", 8080)
    r_time = get_retirement_time()
    
    tribe = Tribe(server, "map1", r_time, num_of_players=10)
    tribe.update_scores()
    tick_seconds(server, r_time)
    tribe.add_time(r_time)
    
    tribe_records = tribe.get_list()
    records = server.get_records()
    compare(records, tribe_records)
    print("✓ test_a_few_zero_records passed")


def test_a_few_records():
    """Test with a few random moves"""
    print("\n=== test_a_few_records ===")
    server = GameServer("127.0.0.1", 8080)
    r_time = get_retirement_time()
    
    tribe = Tribe(server, "map1", r_time, num_of_players=10)
    
    for _ in range(random.randint(100, 350)):
        tribe.randomized_move()
    
    tribe.update_scores()
    tribe.stop()
    tick_seconds(server, r_time)
    tribe.add_time(r_time)
    
    tribe_records = tribe.get_list()
    records = server.get_records()
    compare(records, tribe_records)
    print("✓ test_a_few_records passed")


def test_old_young_tribes_records():
    """Test with two tribes of different ages"""
    print("\n=== test_old_young_tribes_records ===")
    server = GameServer("127.0.0.1", 8080)
    r_time = get_retirement_time()
    
    old_tribe = Tribe(server, "map1", r_time, num_of_players=10, prefix='Elder')
    
    for _ in range(random.randint(50, 200)):
        old_tribe.randomized_move()
    
    old_tribe.update_scores()
    tick_seconds(server, r_time / 2)
    old_tribe.add_time(r_time / 2)
    old_tribe.stop()
    
    young_tribe = Tribe(server, "map1", r_time, num_of_players=10, prefix='Infant')
    
    for _ in range(random.randint(50, 200)):
        young_tribe.randomized_turn()
        ticks = random.randint(100, min(1000, int(r_time * 900)))
        seconds = ticks / 1000
        young_tribe.add_time(seconds)
        old_tribe.add_time(seconds)
        tick_seconds(server, seconds)
    
    young_tribe.update_scores()
    tick_seconds(server, r_time / 2)
    old_tribe.add_time(r_time / 2)
    young_tribe.add_time(r_time / 2)
    
    records = server.get_records()
    tribe_records = old_tribe.get_list()
    compare(records, tribe_records)
    print("✓ test_old_young_tribes_records passed")


def test_a_hundred_records():
    """Test with 100 players"""
    print("\n=== test_a_hundred_records ===")
    server = GameServer("127.0.0.1", 8080)
    r_time = get_retirement_time()
    
    tribe = Tribe(server, "map1", r_time, num_of_players=100)
    
    for _ in range(random.randint(10, 35)):
        tribe.randomized_move()
    
    tribe.update_scores()
    tribe.stop()
    tick_seconds(server, r_time)
    tribe.add_time(r_time)
    
    records = server.get_records()
    tribe_records = tribe.get_list()
    compare(records, tribe_records)
    print("✓ test_a_hundred_records passed")


def test_a_hundred_plus_records():
    """Test with more than 100 players (should return only top 100)"""
    print("\n=== test_a_hundred_plus_records ===")
    server = GameServer("127.0.0.1", 8080)
    r_time = get_retirement_time()
    
    tribe = Tribe(server, "map1", r_time, num_of_players=150)
    
    for _ in range(random.randint(10, 35)):
        tribe.randomized_move()
    
    tribe.update_scores()
    tribe.stop()
    tick_seconds(server, r_time)
    tribe.add_time(r_time)
    
    records = server.get_records()
    tribe_records = tribe.get_list()[:100]
    compare(records, tribe_records)
    print("✓ test_a_hundred_plus_records passed")


def test_two_sequential_tribes_records():
    """Test with two tribes that play sequentially"""
    print("\n=== test_two_sequential_tribes_records ===")
    server = GameServer("127.0.0.1", 8080)
    r_time = get_retirement_time()
    
    red_foxes = Tribe(server, "map1", r_time, num_of_players=50, prefix='Red fox')
    
    for _ in range(random.randint(10, 35)):
        red_foxes.randomized_move()
    
    red_foxes.update_scores()
    red_foxes.stop()
    tick_seconds(server, r_time)
    red_foxes.add_time(r_time)
    
    tribe_records = red_foxes.get_list()
    records = server.get_records()
    compare(records, tribe_records)
    
    orange_raccoons = Tribe(server, "map1", r_time, num_of_players=50, prefix='Orange Raccoon')
    
    for _ in range(random.randint(10, 35)):
        orange_raccoons.randomized_move()
    
    orange_raccoons.update_scores()
    orange_raccoons.stop()
    tick_seconds(server, r_time)
    orange_raccoons.add_time(r_time)
    
    tribe_records.extend(orange_raccoons.get_list())
    tribe_records.sort(key=lambda x: x['score'], reverse=True)
    
    records = server.get_records()
    compare(records, tribe_records)
    print("✓ test_two_sequential_tribes_records passed")


def test_a_records_selection():
    """Test records pagination with start and maxItems parameters"""
    print("\n=== test_a_records_selection ===")
    server = GameServer("127.0.0.1", 8080)
    r_time = get_retirement_time()
    
    # Random parameters
    start = random.randint(0, 30)
    max_items = random.randint(10, 50)
    extra_players = random.randint(5, 60)
    
    print(f"DEBUG: start={start}, max_items={max_items}, extra_players={extra_players}")
    
    tribe = Tribe(server, "map1", r_time, num_of_players=start + extra_players)
    
    for _ in range(random.randint(5, 15)):
        tribe.randomized_move()
    
    tribe.update_scores()
    tribe.stop()
    tick_seconds(server, r_time)
    tribe.add_time(r_time)
    
    end = min(start + extra_players, start + max_items)
    tribe_records = tribe.get_list()[start:end]
    records = server.get_records(start, max_items)
    
    compare(records, tribe_records)
    print("✓ test_a_records_selection passed")


def run_all_tests():
    """Run all tests sequentially"""
    print("=" * 60)
    print("STARTING ALL TESTS")
    print("=" * 60)
    
    # Check server availability
    try:
        response = requests.get("http://127.0.0.1:8080/api/v1/maps", timeout=5)
        print(f"Server health check: status={response.status_code}")
        if response.status_code != 200:
            print(f"WARNING: Server returned {response.status_code}")
    except requests.exceptions.ConnectionError:
        print("ERROR: Cannot connect to server! Make sure the server is running on port 8080")
        print("Start the server with: ./game_server --config-file ./data/config.json --www-root ./static --tick-period 0")
        return
    except Exception as e:
        print(f"ERROR: {e}")
        return
    
    tests = [
        ("test_clean_records", test_clean_records),
        ("test_retirement_one_standing_player", test_retirement_one_standing_player),
        ("test_retirement_one_player", test_retirement_one_player),
        ("test_a_few_zero_records", test_a_few_zero_records),
        ("test_a_few_records", test_a_few_records),
        ("test_old_young_tribes_records", test_old_young_tribes_records),
        ("test_a_hundred_records", test_a_hundred_records),
        ("test_a_hundred_plus_records", test_a_hundred_plus_records),
        ("test_two_sequential_tribes_records", test_two_sequential_tribes_records),
        ("test_a_records_selection", test_a_records_selection),
    ]
    
    passed = 0
    failed = 0
    
    for test_name, test_func in tests:
        try:
            test_func()
            passed += 1
        except Exception as e:
            print(f"\n!!! TEST {test_name} FAILED !!!")
            print(f"Error: {e}")
            import traceback
            traceback.print_exc()
            failed += 1
        print("-" * 40)
    
    print("\n" + "=" * 60)
    print(f"RESULTS: {passed} passed, {failed} failed")
    print("=" * 60)


if __name__ == "__main__":
    run_all_tests()