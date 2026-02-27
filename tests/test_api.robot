*** Settings ***
Library           RequestsLibrary
Library           Collections
Library           JSONLibrary

*** Variables ***
${BASE_URL}       http://127.0.0.1:18080
${USERNAME}       testuser_robot
${PASSWORD}       test123

*** Test Cases ***
Test Ping Endpoint
    [Documentation]    To verify basic connection
    ${response}=    GET    ${BASE_URL}/ping
    Status Should Be    200
    Should Be Equal    ${response.text}    pong

Test Chat Endpoint
    [Documentation]    To verify chat endpoint
    ${response}=    GET    ${BASE_URL}/chat
    Status Should Be    200
    Should Contain    ${response.text}    miaou

Test Register User
    [Documentation]    Registration testing
    &{data}=    Create Dictionary    username=${USERNAME}    password=${PASSWORD}
    ${response}=    POST    ${BASE_URL}/register    data=${data}
    Status Should Be    200
    Log    Response: ${response.text}