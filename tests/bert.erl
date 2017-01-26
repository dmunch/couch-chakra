#!/usr/bin/env escript
%% -*- erlang -*-
%%! -smp enable 

main(List) ->
  lists:map(fun(String) -> 
                T = list_to_term(String),
                B = encode(T),
                io:fwrite(B)
            end, List).
  %io:format("~p", [T]).

list_to_term(String) ->
  {ok, T, _} = erl_scan:string(String++"."),
  case erl_parse:parse_term(T) of
    {ok, Term} ->
      Term;
    {error, Error} ->
      Error
  end.

%%% See http://github.com/mojombo/bert.erl for documentation.
%%% MIT License - Copyright (c) 2009 Tom Preston-Werner <tom@mojombo.com>


%%---------------------------------------------------------------------------
%% Public API

-spec encode(term()) -> binary().

encode(Term) ->
  term_to_binary(encode_term(Term)).

%%-spec decode(binary()) -> term().
%%
%%decode(Bin) ->
%%  decode_term(binary_to_term(Bin)).

%%---------------------------------------------------------------------------
%% Encode

%-spec encode_term(term()) -> term().

encode_term(Term) ->
  case Term of
    [] -> {bert, nil};
    true -> {bert, true};
    false -> {bert, false};
    Dict when is_record(Term, dict, 8) ->
      {bert, dict, dict:to_list(Dict)};
    List when is_list(Term) ->
      lists:map(fun(InnerTerm) -> encode_term(InnerTerm) end, List);
    Tuple when is_tuple(Term) ->
      TList = tuple_to_list(Tuple),
      TList2 = lists:map((fun(InnerTerm) -> encode_term(InnerTerm) end), TList),
      list_to_tuple(TList2);
    _Else -> Term
  end.

%%---------------------------------------------------------------------------
%% Decode
%%
%%-spec decode_term(term()) -> term().
%%
%%decode_term(Term) ->
%%  case Term of
%%    {bert, nil} -> [];
%%    {bert, true} -> true;
%%    {bert, false} -> false;
%%    {bert, dict, Dict} ->
%%      dict:from_list(Dict);
%%    {bert, Other} ->
%%      {bert, Other};
%%    List when is_list(Term) ->
%%      lists:map((fun decode_term/1), List);
%%    Tuple when is_tuple(Term) ->
%%      TList = tuple_to_list(Tuple),
%%      TList2 = lists:map((fun decode_term/1), TList),
%%      list_to_tuple(TList2);
%%    _Else -> Term
%%  end.
