import json
import random
import math

from enum import Enum
from typing import Optional, Tuple, List, Union, Type, KeysView, Any
from dataclasses import dataclass
from collections import defaultdict

from urllib.parse import urljoin

import requests

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
        return f"{self.message}: {json.dumps(self.data)}"


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


    def __enter__(self, **kwargs):
        self.__init__(**kwargs)

    def request(self, method, header, url, **kwargs):
        try:
            req = requests.Request(method, urljoin(self.url, url), headers=header, **kwargs).prepare()
            with requests.Session() as session:
                return session.send(req)
        except Exception as ex:
            print(ex)

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
        request = 'api/v1/game/join'
        header = {'content-type': 'application/json'}
        data = {"userName": player_name, "mapId": map_id}
        res = self.request('POST', header, request, json=data)
        res_json: dict = res.json()

        GameServer.assert_fields('Join game response', ['authToken', 'playerId'], res_json.keys())

        token = res_json['authToken']
        self.validate_token(token)

        player_id = res_json['playerId']
        GameServer.assert_type('Player id', int, player_id)

        return token, player_id

    def add_player(self, player_name: str, map_id: str) -> Tuple[str, int]:
        params = self.join(player_name, map_id)
        # Temporary "fix"
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
        request = '/api/v1/game/player/action'
        header = {'content-type': 'application/json', 'Authorization': f'Bearer {token}'}
        data = {"move": direction}
        res = self.request('POST', header, request, json=data)
        self.validate_response(res)

    def tick(self, ticks: int):
        request = 'api/v1/game/tick'
        header = {'content-type': 'application/json'}
        data = {"timeDelta": ticks}
        res = self.request('POST', header, request, json=data)
        self.validate_response(res)

    def get_records(self, start: int = 0, max_items: int = 100) -> list:
        request = '/api/v1/game/records'
        header = {'content-type': 'application/json'}
        url_params = {'start': start, 'maxItems': max_items}
        res: requests.Response = self.request('GET', header, request, params=url_params)
        self.validate_response(res)
        res_json: list = res.json()
        assert type(res_json) == list

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

    # Wil be rewritten soon
    @staticmethod
    def validate_response(res: requests.Response):
        if res.status_code != 200:
            raise BadRequest('Status code isn\'t OK',
                             {'status code': res.status_code,
                              'response': res.content})

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
        for i in range(0, num_of_players):
            name = f'{prefix} {i}'
            token, player_id = server.join(name, map_id)
            self.players.append(Player(name, token, player_id))

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
        for player in self.players:
            player.update_score(self.server)

    def randomized_turn(self):
        for pl in self.players:
            direction = Direction.random_str()
            self.server.move(pl.token, direction)

    def randomized_move(self):
        self.randomized_turn()
        ticks = random.randint(100, min(10000, int(self.r_time*900)))
        seconds = ticks / 1000
        self.add_time(seconds)
        self.__tick_seconds(seconds)

    def stop(self):
        for pl in self.players:
            self.server.move(pl.token, '')


def compare(records: List[dict], tribe_records: List[dict]):
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


def test_a_hundred_records(server, map_id):
    tribe = Tribe(server, map_id, num_of_players=100)
    r_time = 15.0

    for _ in range(0, random.randint(10, 35)):
        tribe.randomized_move()

    tribe.update_scores()
    tribe.stop()
    server.tick(int(r_time * 1000))
    tribe.add_time(r_time)

    records = server.get_records()
    tribe_records = tribe.get_list()

    compare(records, tribe_records)

if __name__ == "__main__":
    server = GameServer("127.0.0.1", 8080)
    test_a_hundred_records(server, "map1")
